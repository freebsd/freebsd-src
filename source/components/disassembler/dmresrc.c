/*******************************************************************************
 *
 * Module Name: dmresrc.c - Resource Descriptor disassembly
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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
#include "amlcode.h"
#include "acdisasm.h"


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbresrc")


/* Dispatch tables for Resource disassembly functions */

static ACPI_RESOURCE_HANDLER    AcpiGbl_DmResourceDispatch [] =
{
    /* Small descriptors */

    NULL,                           /* 0x00, Reserved */
    NULL,                           /* 0x01, Reserved */
    NULL,                           /* 0x02, Reserved */
    NULL,                           /* 0x03, Reserved */
    AcpiDmIrqDescriptor,            /* 0x04, ACPI_RESOURCE_NAME_IRQ_FORMAT */
    AcpiDmDmaDescriptor,            /* 0x05, ACPI_RESOURCE_NAME_DMA_FORMAT */
    AcpiDmStartDependentDescriptor, /* 0x06, ACPI_RESOURCE_NAME_START_DEPENDENT */
    AcpiDmEndDependentDescriptor,   /* 0x07, ACPI_RESOURCE_NAME_END_DEPENDENT */
    AcpiDmIoDescriptor,             /* 0x08, ACPI_RESOURCE_NAME_IO_PORT */
    AcpiDmFixedIoDescriptor,        /* 0x09, ACPI_RESOURCE_NAME_FIXED_IO_PORT */
    AcpiDmFixedDmaDescriptor,       /* 0x0A, ACPI_RESOURCE_NAME_FIXED_DMA */
    NULL,                           /* 0x0B, Reserved */
    NULL,                           /* 0x0C, Reserved */
    NULL,                           /* 0x0D, Reserved */
    AcpiDmVendorSmallDescriptor,    /* 0x0E, ACPI_RESOURCE_NAME_SMALL_VENDOR */
    NULL,                           /* 0x0F, ACPI_RESOURCE_NAME_END_TAG (not used) */

    /* Large descriptors */

    NULL,                           /* 0x00, Reserved */
    AcpiDmMemory24Descriptor,       /* 0x01, ACPI_RESOURCE_NAME_MEMORY_24 */
    AcpiDmGenericRegisterDescriptor,/* 0x02, ACPI_RESOURCE_NAME_GENERIC_REGISTER */
    NULL,                           /* 0x03, Reserved */
    AcpiDmVendorLargeDescriptor,    /* 0x04, ACPI_RESOURCE_NAME_LARGE_VENDOR */
    AcpiDmMemory32Descriptor,       /* 0x05, ACPI_RESOURCE_NAME_MEMORY_32 */
    AcpiDmFixedMemory32Descriptor,  /* 0x06, ACPI_RESOURCE_NAME_FIXED_MEMORY_32 */
    AcpiDmDwordDescriptor,          /* 0x07, ACPI_RESOURCE_NAME_DWORD_ADDRESS_SPACE */
    AcpiDmWordDescriptor,           /* 0x08, ACPI_RESOURCE_NAME_WORD_ADDRESS_SPACE */
    AcpiDmInterruptDescriptor,      /* 0x09, ACPI_RESOURCE_NAME_EXTENDED_XRUPT */
    AcpiDmQwordDescriptor,          /* 0x0A, ACPI_RESOURCE_NAME_QWORD_ADDRESS_SPACE */
    AcpiDmExtendedDescriptor,       /* 0x0B, ACPI_RESOURCE_NAME_EXTENDED_ADDRESS_SPACE */
    AcpiDmGpioDescriptor,           /* 0x0C, ACPI_RESOURCE_NAME_GPIO */
    NULL,                           /* 0x0D, Reserved */
    AcpiDmSerialBusDescriptor       /* 0x0E, ACPI_RESOURCE_NAME_SERIAL_BUS */
};


/* Only used for single-threaded applications */
/* TBD: remove when name is passed as parameter to the dump functions */

static UINT32               ResourceName;


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDescriptorName
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit a name for the descriptor if one is present (indicated
 *              by the name being changed from the default name.) A name is only
 *              emitted if a reference to the descriptor has been made somewhere
 *              in the original ASL code.
 *
 ******************************************************************************/

void
AcpiDmDescriptorName (
    void)
{

    if (ResourceName == ACPI_DEFAULT_RESNAME)
    {
        return;
    }

    AcpiOsPrintf ("%4.4s", (char *) &ResourceName);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpInteger*
 *
 * PARAMETERS:  Value               - Value to emit
 *              Name                - Associated name (emitted as a comment)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Integer output helper functions
 *
 ******************************************************************************/

void
AcpiDmDumpInteger8 (
    UINT8                   Value,
    const char              *Name)
{
    AcpiOsPrintf ("0x%2.2X,               // %s\n", Value, Name);
}

void
AcpiDmDumpInteger16 (
    UINT16                  Value,
    const char              *Name)
{
    AcpiOsPrintf ("0x%4.4X,             // %s\n", Value, Name);
}

void
AcpiDmDumpInteger32 (
    UINT32                  Value,
    const char              *Name)
{
    AcpiOsPrintf ("0x%8.8X,         // %s\n", Value, Name);
}

void
AcpiDmDumpInteger64 (
    UINT64                  Value,
    const char              *Name)
{
    AcpiOsPrintf ("0x%8.8X%8.8X, // %s\n", ACPI_FORMAT_UINT64 (Value), Name);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmBitList
 *
 * PARAMETERS:  Mask            - 16-bit value corresponding to 16 interrupt
 *                                or DMA values
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump a bit mask as a list of individual interrupt/DMA levels.
 *
 ******************************************************************************/

void
AcpiDmBitList (
    UINT16                  Mask)
{
    UINT32                  i;
    BOOLEAN                 Previous = FALSE;


    /* Open the initializer list */

    AcpiOsPrintf ("{");

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
            AcpiOsPrintf ("%u", i);
        }

        Mask >>= 1;
    }

    /* Close list */

    AcpiOsPrintf ("}\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmResourceTemplate
 *
 * PARAMETERS:  Info            - Curent parse tree walk info
 *              ByteData        - Pointer to the byte list data
 *              ByteCount       - Length of the byte list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the contents of a Resource Template containing a set of
 *              Resource Descriptors.
 *
 ******************************************************************************/

void
AcpiDmResourceTemplate (
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *ByteData,
    UINT32                  ByteCount)
{
    ACPI_STATUS             Status;
    UINT32                  CurrentByteOffset;
    UINT8                   ResourceType;
    UINT32                  ResourceLength;
    void                    *Aml;
    UINT32                  Level;
    BOOLEAN                 DependentFns = FALSE;
    UINT8                   ResourceIndex;
    ACPI_NAMESPACE_NODE     *Node;


    if (Op->Asl.AmlOpcode != AML_FIELD_OP)
    {
        Info->MappingOp = Op;
    }

    Level = Info->Level;
    ResourceName = ACPI_DEFAULT_RESNAME;
    Node = Op->Common.Node;
    if (Node)
    {
        Node = Node->Child;
    }

    for (CurrentByteOffset = 0; CurrentByteOffset < ByteCount;)
    {
        Aml = &ByteData[CurrentByteOffset];

        /* Get the descriptor type and length */

        ResourceType = AcpiUtGetResourceType (Aml);
        ResourceLength = AcpiUtGetResourceLength (Aml);

        /* Validate the Resource Type and Resource Length */

        Status = AcpiUtValidateResource (NULL, Aml, &ResourceIndex);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf (
                "/*** Could not validate Resource, type (%X) %s***/\n",
                ResourceType, AcpiFormatException (Status));
            return;
        }

        /* Point to next descriptor */

        CurrentByteOffset += AcpiUtGetDescriptorLength (Aml);

        /* Descriptor pre-processing */

        switch (ResourceType)
        {
        case ACPI_RESOURCE_NAME_START_DEPENDENT:

            /* Finish a previous StartDependentFns */

            if (DependentFns)
            {
                Level--;
                AcpiDmIndent (Level);
                AcpiOsPrintf ("}\n");
            }
            break;

        case ACPI_RESOURCE_NAME_END_DEPENDENT:

            Level--;
            DependentFns = FALSE;
            break;

        case ACPI_RESOURCE_NAME_END_TAG:

            /* Normal exit, the resource list is finished */

            if (DependentFns)
            {
                /*
                 * Close an open StartDependentDescriptor. This indicates a
                 * missing EndDependentDescriptor.
                 */
                Level--;
                DependentFns = FALSE;

                /* Go ahead and insert EndDependentFn() */

                AcpiDmEndDependentDescriptor (Info, Aml, ResourceLength, Level);

                AcpiDmIndent (Level);
                AcpiOsPrintf (
                    "/*** Disassembler: inserted "
                    "missing EndDependentFn () ***/\n");
            }
            return;

        default:

            break;
        }

        /* Disassemble the resource structure */

        if (Node)
        {
            ResourceName = Node->Name.Integer;
            Node = Node->Peer;
        }

        AcpiGbl_DmResourceDispatch [ResourceIndex] (
            Info, Aml, ResourceLength, Level);

        /* Descriptor post-processing */

        if (ResourceType == ACPI_RESOURCE_NAME_START_DEPENDENT)
        {
            DependentFns = TRUE;
            Level++;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsResourceTemplate
 *
 * PARAMETERS:  WalkState           - Current walk info
 *              Op                  - Buffer Op to be examined
 *
 * RETURN:      Status. AE_OK if valid template
 *
 * DESCRIPTION: Walk a byte list to determine if it consists of a valid set
 *              of resource descriptors. Nothing is output.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDmIsResourceTemplate (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *NextOp;
    UINT8                   *Aml;
    UINT8                   *EndAml;
    ACPI_SIZE               Length;


    /* This op must be a buffer */

    if (Op->Common.AmlOpcode != AML_BUFFER_OP)
    {
        return (AE_TYPE);
    }

    /* Get the ByteData list and length */

    NextOp = Op->Common.Value.Arg;
    if (!NextOp)
    {
        AcpiOsPrintf ("NULL byte list in buffer\n");
        return (AE_TYPE);
    }

    NextOp = NextOp->Common.Next;
    if (!NextOp)
    {
        return (AE_TYPE);
    }

    Aml = NextOp->Named.Data;
    Length = (ACPI_SIZE) NextOp->Common.Value.Integer;

    /* Walk the byte list, abort on any invalid descriptor type or length */

    Status = AcpiUtWalkAmlResources (WalkState, Aml, Length,
        NULL, ACPI_CAST_INDIRECT_PTR (void, &EndAml));
    if (ACPI_FAILURE (Status))
    {
        return (AE_TYPE);
    }

    /*
     * For the resource template to be valid, one EndTag must appear
     * at the very end of the ByteList, not before. (For proper disassembly
     * of a ResourceTemplate, the buffer must not have any extra data after
     * the EndTag.)
     */
    if ((Aml + Length - sizeof (AML_RESOURCE_END_TAG)) != EndAml)
    {
        return (AE_AML_NO_RESOURCE_END_TAG);
    }

    /*
     * All resource descriptors are valid, therefore this list appears
     * to be a valid resource template
     */
    return (AE_OK);
}
