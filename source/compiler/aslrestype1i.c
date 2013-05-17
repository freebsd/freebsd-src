/******************************************************************************
 *
 * Module Name: aslrestype1i - Small I/O-related resource descriptors
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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


#include "aslcompiler.h"
#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype1i")

/*
 * This module contains the I/O-related small resource descriptors:
 *
 * DMA
 * FixedDMA
 * FixedIO
 * IO
 * IRQ
 * IRQNoFlags
 */

/*******************************************************************************
 *
 * FUNCTION:    RsDoDmaDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "DMA" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoDmaDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;
    UINT8                   DmaChannelMask = 0;
    UINT8                   DmaChannels = 0;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_DMA));

    Descriptor = Rnode->Buffer;
    Descriptor->Dma.DescriptorType  = ACPI_RESOURCE_NAME_DMA |
                                        ASL_RDESC_DMA_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* DMA type */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 5, 0);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_DMATYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 5, 2);
            break;

        case 1: /* Bus Master */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_BUSMASTER,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 2);
            break;

        case 2: /* Xfer Type (transfer width) */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 0, 0);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_XFERTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 0, 2);
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* All DMA channel bytes are handled here, after flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Up to 8 channels can be specified in the list */

                DmaChannels++;
                if (DmaChannels > 8)
                {
                    AslError (ASL_ERROR, ASL_MSG_DMA_LIST,
                        InitializerOp, NULL);
                    return (Rnode);
                }

                /* Only DMA channels 0-7 are allowed (mask is 8 bits) */

                if (InitializerOp->Asl.Value.Integer > 7)
                {
                    AslError (ASL_ERROR, ASL_MSG_DMA_CHANNEL,
                        InitializerOp, NULL);
                }

                /* Build the mask */

                DmaChannelMask |=
                    (1 << ((UINT8) InitializerOp->Asl.Value.Integer));
            }

            if (i == 4) /* case 4: First DMA byte */
            {
                /* Check now for duplicates in list */

                RsCheckListForDuplicates (InitializerOp);

                /* Create a named field at the start of the list */

                RsCreateByteField (InitializerOp, ACPI_RESTAG_DMA,
                    CurrentByteOffset +
                    ASL_RESDESC_OFFSET (Dma.DmaChannelMask));
            }
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Now we can set the channel mask */

    Descriptor->Dma.DmaChannelMask = DmaChannelMask;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoFixedDmaDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "FixedDMA" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoFixedDmaDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_FIXED_DMA));

    Descriptor = Rnode->Buffer;
    Descriptor->FixedDma.DescriptorType =
        ACPI_RESOURCE_NAME_FIXED_DMA | ASL_RDESC_FIXED_DMA_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* DMA Request Lines [WORD] (_DMA) */

            Descriptor->FixedDma.RequestLines = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_DMA,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedDma.RequestLines));
            break;

        case 1: /* DMA Channel [WORD] (_TYP) */

            Descriptor->FixedDma.Channels = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_DMATYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedDma.Channels));
            break;

        case 2: /* Transfer Width [BYTE] (_SIZ) */

            Descriptor->FixedDma.Width = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_XFERTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedDma.Width));
            break;

        case 3: /* Descriptor Name (optional) */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:    /* Ignore any extra nodes */

            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoFixedIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "FixedIO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoFixedIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *AddressOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_FIXED_IO));

    Descriptor = Rnode->Buffer;
    Descriptor->Io.DescriptorType  = ACPI_RESOURCE_NAME_FIXED_IO |
                                      ASL_RDESC_FIXED_IO_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Base Address */

            Descriptor->FixedIo.Address =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_BASEADDRESS,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedIo.Address));
            AddressOp = InitializerOp;
            break;

        case 1: /* Length */

            Descriptor->FixedIo.AddressLength =
                (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedIo.AddressLength));
            break;

        case 2: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Error checks */

    if (Descriptor->FixedIo.Address > 0x03FF)
    {
        AslError (ASL_WARNING, ASL_MSG_ISA_ADDRESS, AddressOp, NULL);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "IO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *AlignOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_IO));

    Descriptor = Rnode->Buffer;
    Descriptor->Io.DescriptorType  = ACPI_RESOURCE_NAME_IO |
                                      ASL_RDESC_IO_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Decode size */

            RsSetFlagBits (&Descriptor->Io.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Flags), 0);
            break;

        case 1:  /* Min Address */

            Descriptor->Io.Minimum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Minimum));
            MinOp = InitializerOp;
            break;

        case 2: /* Max Address */

            Descriptor->Io.Maximum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Maximum));
            MaxOp = InitializerOp;
            break;

        case 3: /* Alignment */

            Descriptor->Io.Alignment =
                (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ALIGNMENT,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Alignment));
            AlignOp = InitializerOp;
            break;

        case 4: /* Length */

            Descriptor->Io.AddressLength =
                (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 5: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Align values */

    RsSmallAddressCheck (ACPI_RESOURCE_NAME_IO,
        Descriptor->Io.Minimum,
        Descriptor->Io.Maximum,
        Descriptor->Io.AddressLength,
        Descriptor->Io.Alignment,
        MinOp, MaxOp, LengthOp, AlignOp, Op);

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoIrqDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "IRQ" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoIrqDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  Interrupts = 0;
    UINT16                  IrqMask = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_IRQ));

    /* Length = 3 (with flag byte) */

    Descriptor = Rnode->Buffer;
    Descriptor->Irq.DescriptorType  = ACPI_RESOURCE_NAME_IRQ |
                                      (ASL_RDESC_IRQ_SIZE + 0x01);

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Interrupt Type (or Mode - edge/level) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 0);
            break;

        case 1: /* Interrupt Level (or Polarity - Active high/low) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTLEVEL,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 3);
            break;

        case 2: /* Share Type - Default: exclusive (0) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTSHARE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 4);
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* All IRQ bytes are handled here, after the flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Up to 16 interrupts can be specified in the list */

                Interrupts++;
                if (Interrupts > 16)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_LIST,
                        InitializerOp, NULL);
                    return (Rnode);
                }

                /* Only interrupts 0-15 are allowed (mask is 16 bits) */

                if (InitializerOp->Asl.Value.Integer > 15)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_NUMBER,
                        InitializerOp, NULL);
                }
                else
                {
                    IrqMask |= (1 << (UINT8) InitializerOp->Asl.Value.Integer);
                }
            }

            /* Case 4: First IRQ value in list */

            if (i == 4)
            {
                /* Check now for duplicates in list */

                RsCheckListForDuplicates (InitializerOp);

                /* Create a named field at the start of the list */

                RsCreateWordField (InitializerOp, ACPI_RESTAG_INTERRUPT,
                    CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.IrqMask));
            }
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Now we can set the channel mask */

    Descriptor->Irq.IrqMask = IrqMask;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoIrqNoFlagsDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "IRQNoFlags" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoIrqNoFlagsDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  IrqMask = 0;
    UINT32                  Interrupts = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_IRQ_NOFLAGS));

    Descriptor = Rnode->Buffer;
    Descriptor->Irq.DescriptorType  = ACPI_RESOURCE_NAME_IRQ |
                                      ASL_RDESC_IRQ_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* IRQ bytes are handled here, after the flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Up to 16 interrupts can be specified in the list */

                Interrupts++;
                if (Interrupts > 16)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_LIST,
                        InitializerOp, NULL);
                    return (Rnode);
                }

                /* Only interrupts 0-15 are allowed (mask is 16 bits) */

                if (InitializerOp->Asl.Value.Integer > 15)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_NUMBER,
                        InitializerOp, NULL);
                }
                else
                {
                    IrqMask |= (1 << ((UINT8) InitializerOp->Asl.Value.Integer));
                }
            }

            /* Case 1: First IRQ value in list */

            if (i == 1)
            {
                /* Check now for duplicates in list */

                RsCheckListForDuplicates (InitializerOp);

                /* Create a named field at the start of the list */

                RsCreateWordField (InitializerOp, ACPI_RESTAG_INTERRUPT,
                    CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.IrqMask));
            }
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Now we can set the interrupt mask */

    Descriptor->Irq.IrqMask = IrqMask;
    return (Rnode);
}
