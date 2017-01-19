/*******************************************************************************
 *
 * Module Name: dmresrcs.c - "Small" Resource Descriptor disassembly
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
 * All rights reserved.
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
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "acpi.h"
#include "accommon.h"
#include "acdisasm.h"


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbresrcs")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIrqDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a IRQ descriptor, either Irq() or IrqNoFlags()
 *
 ******************************************************************************/

void
AcpiDmIrqDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("%s (",
        AcpiGbl_IrqDecode [ACPI_GET_1BIT_FLAG (Length)]);

    /* Decode flags byte if present */

    if (Length & 1)
    {
        AcpiOsPrintf ("%s, %s, %s, ",
            AcpiGbl_HeDecode [ACPI_GET_1BIT_FLAG (Resource->Irq.Flags)],
            AcpiGbl_LlDecode [ACPI_EXTRACT_1BIT_FLAG (Resource->Irq.Flags, 3)],
            AcpiGbl_ShrDecode [ACPI_EXTRACT_2BIT_FLAG (Resource->Irq.Flags, 4)]);
    }

    /* Insert a descriptor name */

    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");

    AcpiDmIndent (Level + 1);
    AcpiDmBitList (Resource->Irq.IrqMask);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDmaDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a DMA descriptor
 *
 ******************************************************************************/

void
AcpiDmDmaDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("DMA (%s, %s, %s, ",
        AcpiGbl_TypDecode [ACPI_EXTRACT_2BIT_FLAG (Resource->Dma.Flags, 5)],
        AcpiGbl_BmDecode  [ACPI_EXTRACT_1BIT_FLAG (Resource->Dma.Flags, 2)],
        AcpiGbl_SizDecode [ACPI_GET_2BIT_FLAG (Resource->Dma.Flags)]);

    /* Insert a descriptor name */

    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");

    AcpiDmIndent (Level + 1);
    AcpiDmBitList (Resource->Dma.DmaChannelMask);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFixedDmaDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a FixedDMA descriptor
 *
 ******************************************************************************/

void
AcpiDmFixedDmaDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("FixedDMA (0x%4.4X, 0x%4.4X, ",
        Resource->FixedDma.RequestLines,
        Resource->FixedDma.Channels);

    if (Resource->FixedDma.Width <= 5)
    {
        AcpiOsPrintf ("%s, ",
            AcpiGbl_DtsDecode [Resource->FixedDma.Width]);
    }
    else
    {
        AcpiOsPrintf ("%X /* INVALID DMA WIDTH */, ",
            Resource->FixedDma.Width);
    }

    /* Insert a descriptor name */

    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIoDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode an IO descriptor
 *
 ******************************************************************************/

void
AcpiDmIoDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("IO (%s,\n",
        AcpiGbl_IoDecode [ACPI_GET_1BIT_FLAG (Resource->Io.Flags)]);

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger16 (Resource->Io.Minimum, "Range Minimum");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger16 (Resource->Io.Maximum, "Range Maximum");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger8 (Resource->Io.Alignment, "Alignment");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger8 (Resource->Io.AddressLength, "Length");

    /* Insert a descriptor name */

    AcpiDmIndent (Level + 1);
    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFixedIoDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Fixed IO descriptor
 *
 ******************************************************************************/

void
AcpiDmFixedIoDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("FixedIO (\n");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger16 (Resource->FixedIo.Address, "Address");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger8 (Resource->FixedIo.AddressLength, "Length");

    /* Insert a descriptor name */

    AcpiDmIndent (Level + 1);
    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmStartDependentDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Start Dependendent functions descriptor
 *
 ******************************************************************************/

void
AcpiDmStartDependentDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);

    if (Length & 1)
    {
        AcpiOsPrintf ("StartDependentFn (0x%2.2X, 0x%2.2X)\n",
            (UINT32) ACPI_GET_2BIT_FLAG (Resource->StartDpf.Flags),
            (UINT32) ACPI_EXTRACT_2BIT_FLAG (Resource->StartDpf.Flags, 2));
    }
    else
    {
        AcpiOsPrintf ("StartDependentFnNoPri ()\n");
    }

    AcpiDmIndent (Level);
    AcpiOsPrintf ("{\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmEndDependentDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode an End Dependent functions descriptor
 *
 ******************************************************************************/

void
AcpiDmEndDependentDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("}\n");
    AcpiDmIndent (Level);
    AcpiOsPrintf ("EndDependentFn ()\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmVendorSmallDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Vendor Small Descriptor
 *
 ******************************************************************************/

void
AcpiDmVendorSmallDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmVendorCommon ("Short",
        ACPI_ADD_PTR (UINT8, Resource, sizeof (AML_RESOURCE_SMALL_HEADER)),
        Length, Level);
}
