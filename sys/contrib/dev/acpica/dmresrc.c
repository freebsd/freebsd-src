/*******************************************************************************
 *
 * Module Name: dmresrc.c - Resource Descriptor disassembly
 *              $Revision: 5 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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


#include "acpi.h"
#include "amlcode.h"
#include "acdisasm.h"

#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbresrc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmBitList
 *
 * PARAMETERS:  Mask            - 16-bit value corresponding to 16 interrupt
 *                                or DMA values
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump a bit mask as a list of individual interrupt/dma levels.
 *
 ******************************************************************************/

void
AcpiDmBitList (
    UINT16                  Mask)
{
    UINT32                  i;
    BOOLEAN                 Previous = FALSE;


    /* Open the initializer list */

    AcpiOsPrintf (") {");

    /* Examine each bit */

    for (i = 0; i < 16; i++)
    {
        /* Only interested in bits that are set to 1 */

        if (Mask & 1)
        {
            if (Previous)
            {
                AcpiOsPrintf (",");
            }
            Previous = TRUE;
            AcpiOsPrintf ("%d", i);
        }

        Mask >>= 1;
    }

    /* Close list */

    AcpiOsPrintf ("}\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmResourceDescriptor
 *
 * PARAMETERS:  Info            - Curent parse tree walk info
 *              ByteData        - Pointer to the byte list data
 *              ByteCount       - Length of the byte list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the contents of one ResourceTemplate descriptor.
 *
 ******************************************************************************/

void
AcpiDmResourceDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    UINT8                   *ByteData,
    UINT32                  ByteCount)
{
    NATIVE_UINT             CurrentByteOffset;
    UINT8                   CurrentByte;
    UINT8                   DescriptorId;
    UINT32                  Length;
    void                    *DescriptorBody;
    UINT32                  Level;
    BOOLEAN                 DependentFns = FALSE;


    Level = Info->Level;

    for (CurrentByteOffset = 0; CurrentByteOffset < ByteCount; )
    {
        CurrentByte = ByteData[CurrentByteOffset];
        DescriptorBody = &ByteData[CurrentByteOffset];

        if (CurrentByte & ACPI_RDESC_TYPE_LARGE)
        {
            DescriptorId = CurrentByte;
            Length = (* (ACPI_CAST_PTR (UINT16, &ByteData[CurrentByteOffset + 1])));
            CurrentByteOffset += 3;
        }
        else
        {
            DescriptorId = (UINT8) (CurrentByte & 0xF8);
            Length = (ByteData[CurrentByteOffset] & 0x7);
            CurrentByteOffset += 1;
        }

        CurrentByteOffset += (NATIVE_UINT) Length;

        /* Determine type of resource */

        switch (DescriptorId)
        {
        /*
         * "Small" type descriptors
         */
        case ACPI_RDESC_TYPE_IRQ_FORMAT:

            AcpiDmIrqDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_DMA_FORMAT:

            AcpiDmDmaDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_START_DEPENDENT:

            /* Finish a previous StartDependentFns */

            if (DependentFns)
            {
                Level--;
                AcpiDmIndent (Level);
                AcpiOsPrintf ("}\n");
            }

            AcpiDmStartDependentDescriptor (DescriptorBody, Length, Level);
            DependentFns = TRUE;
            Level++;
            break;


        case ACPI_RDESC_TYPE_END_DEPENDENT:

            Level--;
            DependentFns = FALSE;
            AcpiDmEndDependentDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_IO_PORT:

            AcpiDmIoDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_FIXED_IO_PORT:

            AcpiDmFixedIoDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_SMALL_VENDOR:

            AcpiDmVendorSmallDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_END_TAG:

            if (DependentFns)
            {
                /*
                 * Close an open StartDependentDescriptor.  This indicates a missing
                 * EndDependentDescriptor.
                 */
                Level--;
                DependentFns = FALSE;
                AcpiDmIndent (Level);
                AcpiOsPrintf ("}\n");
                AcpiDmIndent (Level);

                AcpiOsPrintf ("/*** Missing EndDependentFunctions descriptor */");

                /*
                 * We could fix the problem, but then the ASL would not match the AML
                 * So, we don't do this:
                 * AcpiDmEndDependentDescriptor (DescriptorBody, Length, Level);
                 */
            }
            return;


        /*
         * "Large" type descriptors
         */
        case ACPI_RDESC_TYPE_MEMORY_24:

            AcpiDmMemory24Descriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_GENERAL_REGISTER:

            AcpiDmGenericRegisterDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_LARGE_VENDOR:

            AcpiDmVendorLargeDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_MEMORY_32:

            AcpiDmMemory32Descriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_FIXED_MEMORY_32:

            AcpiDmFixedMem32Descriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_DWORD_ADDRESS_SPACE:

            AcpiDmDwordDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_WORD_ADDRESS_SPACE:

            AcpiDmWordDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_EXTENDED_XRUPT:

            AcpiDmInterruptDescriptor (DescriptorBody, Length, Level);
            break;


        case ACPI_RDESC_TYPE_QWORD_ADDRESS_SPACE:

            AcpiDmQwordDescriptor (DescriptorBody, Length, Level);
            break;


        default:
            /*
             * Anything else is unrecognized.
             *
             * Since the entire resource buffer has been already walked and
             * validated, this is a very serious error indicating that someone
             * overwrote the buffer.
             */
            AcpiOsPrintf ("/* Unknown Resource type (%X) */\n", DescriptorId);
            return;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsResourceDescriptor
 *
 * PARAMETERS:  Op          - Buffer Op to be examined
 *
 * RETURN:      TRUE if this Buffer Op contains a valid resource
 *              descriptor.
 *
 * DESCRIPTION: Walk a byte list to determine if it consists of a valid set
 *              of resource descriptors.  Nothing is output.
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsResourceDescriptor (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;
    ACPI_PARSE_OBJECT       *NextOp;
    NATIVE_UINT             CurrentByteOffset;
    UINT8                   CurrentByte;
    UINT8                   DescriptorId;
    UINT32                  Length;


    /* This op must be a buffer */

    if (Op->Common.AmlOpcode != AML_BUFFER_OP)
    {
        return FALSE;
    }

    /* Get to the ByteData list */

    NextOp = Op->Common.Value.Arg;
    NextOp = NextOp->Common.Next;
    if (!NextOp)
    {
        return (FALSE);
    }

    /* Extract the data pointer and data length */

    ByteCount = NextOp->Common.Value.Integer32;
    ByteData = NextOp->Named.Data;

    /* The list must have a valid END_TAG */

    if (ByteData[ByteCount-2] != (ACPI_RDESC_TYPE_END_TAG | 1))
    {
        return FALSE;
    }

    /*
     * Walk the byte list.  Abort on any invalid descriptor ID or
     * or length
     */
    for (CurrentByteOffset = 0; CurrentByteOffset < ByteCount; )
    {
        CurrentByte = ByteData[CurrentByteOffset];

        /* Large or small resource? */

        if (CurrentByte & ACPI_RDESC_TYPE_LARGE)
        {
            DescriptorId = CurrentByte;
            Length = (* (ACPI_CAST_PTR (UINT16, (&ByteData[CurrentByteOffset + 1]))));
            CurrentByteOffset += 3;
        }
        else
        {
            DescriptorId = (UINT8) (CurrentByte & 0xF8);
            Length = (ByteData[CurrentByteOffset] & 0x7);
            CurrentByteOffset += 1;
        }

        CurrentByteOffset += (NATIVE_UINT) Length;

        /* Determine type of resource */

        switch (DescriptorId)
        {
        /*
         * "Small" type descriptors
         */
        case ACPI_RDESC_TYPE_IRQ_FORMAT:
        case ACPI_RDESC_TYPE_DMA_FORMAT:
        case ACPI_RDESC_TYPE_START_DEPENDENT:
        case ACPI_RDESC_TYPE_END_DEPENDENT:
        case ACPI_RDESC_TYPE_IO_PORT:
        case ACPI_RDESC_TYPE_FIXED_IO_PORT:
        case ACPI_RDESC_TYPE_SMALL_VENDOR:
        /*
         * "Large" type descriptors
         */
        case ACPI_RDESC_TYPE_MEMORY_24:
        case ACPI_RDESC_TYPE_GENERAL_REGISTER:
        case ACPI_RDESC_TYPE_LARGE_VENDOR:
        case ACPI_RDESC_TYPE_MEMORY_32:
        case ACPI_RDESC_TYPE_FIXED_MEMORY_32:
        case ACPI_RDESC_TYPE_DWORD_ADDRESS_SPACE:
        case ACPI_RDESC_TYPE_WORD_ADDRESS_SPACE:
        case ACPI_RDESC_TYPE_EXTENDED_XRUPT:
        case ACPI_RDESC_TYPE_QWORD_ADDRESS_SPACE:

            /* Valid descriptor ID, keep going */

            break;


        case ACPI_RDESC_TYPE_END_TAG:

            /* We must be at the end of the ByteList */

            if (CurrentByteOffset != ByteCount)
            {
                return (FALSE);
            }

            /* All descriptors/lengths valid, this is a valid descriptor */

            return (TRUE);


        default:

            /* Bad descriptor, abort */

            return (FALSE);
        }
    }

    /* Did not find an END_TAG, something seriously wrong */

    return (FALSE);
}


#endif
