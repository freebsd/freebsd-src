
/******************************************************************************
 *
 * Module Name: aslrestype1 - Short (type1) resource templates and descriptors
 *              $Revision: 25 $
 *
 *****************************************************************************/

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


#include "aslcompiler.h"
#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype1")


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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;
    UINT8                   DmaChannelMask = 0;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_DMA_FORMAT_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Dma.DescriptorType  = ACPI_RDESC_TYPE_DMA_FORMAT |
                                        ASL_RDESC_DMA_SIZE;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* DMA type */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DMATYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 5);
            break;

        case 1: /* Bus Master */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_BUSMASTER,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 2);
            break;

        case 2: /* Xfer Type (transfer width) */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 0, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_XFERTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 0);
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* All DMA channel bytes are handled here, after the flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                DmaChannelMask |= (1 << ((UINT8) InitializerOp->Asl.Value.Integer));
            }

            if (i == 4) /* case 4: First DMA byte */
            {
                RsCreateByteField (InitializerOp, ASL_RESNAME_DMA,
                                    CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.DmaChannelMask));
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
 * FUNCTION:    RsDoEndDependentDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "EndDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoEndDependentDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    ASL_RESOURCE_DESC       *Descriptor;
    ASL_RESOURCE_NODE       *Rnode;


    Rnode = RsAllocateResourceNode (sizeof (ASL_END_DEPENDENT_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->End.DescriptorType  = ACPI_RDESC_TYPE_END_DEPENDENT |
                                        ASL_RDESC_END_DEPEND_SIZE;
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_FIXED_IO_PORT_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Iop.DescriptorType  = ACPI_RDESC_TYPE_FIXED_IO_PORT |
                                        ASL_RDESC_FIXED_IO_SIZE;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Base Address */

            Descriptor->Fio.BaseAddress = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_BASEADDRESS,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Fio.BaseAddress));
            break;

        case 1: /* Length */

            Descriptor->Fio.Length = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Fio.Length));
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_IO_PORT_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Iop.DescriptorType  = ACPI_RDESC_TYPE_IO_PORT |
                                        ASL_RDESC_IO_SIZE;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Decode size */

            RsSetFlagBits (&Descriptor->Iop.Information, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DECODE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Iop.Information), 0);
            break;

        case 1:  /* Min Address */

            Descriptor->Iop.AddressMin = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Iop.AddressMin));
            break;

        case 2: /* Max Address */

            Descriptor->Iop.AddressMax = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Iop.AddressMax));
            break;

        case 3: /* Alignment */

            Descriptor->Iop.Alignment = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_ALIGNMENT,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Iop.Alignment));
            break;

        case 4: /* Length */

            Descriptor->Iop.Length = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Iop.Length));
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;
    UINT16                  IrqMask = 0;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_IRQ_FORMAT_DESC));

    /* Length = 3 (with flag byte) */

    Descriptor = Rnode->Buffer;
    Descriptor->Irq.DescriptorType  = ACPI_RDESC_TYPE_IRQ_FORMAT |
                                        (ASL_RDESC_IRQ_SIZE + 0x01);

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Interrupt Type (or Mode - edge/level) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ASL_RESNAME_INTERRUPTTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 0);
            break;

        case 1: /* Interrupt Level (or Polarity - Active high/low) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_INTERRUPTLEVEL,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 3);
            break;

        case 2: /* Share Type - Default: exclusive (0) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_INTERRUPTSHARE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 4);
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* All IRQ bytes are handled here, after the flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                IrqMask |= (1 << (UINT8) InitializerOp->Asl.Value.Integer);
            }

            if (i == 4) /* case 4: First IRQ byte */
            {
                RsCreateByteField (InitializerOp, ASL_RESNAME_INTERRUPT,
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;
    UINT16                  IrqMask = 0;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_IRQ_NOFLAGS_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Irq.DescriptorType  = ACPI_RDESC_TYPE_IRQ_FORMAT |
                                        ASL_RDESC_IRQ_SIZE;

    /*
     * Process all child initialization nodes
     */
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
                IrqMask |= (1 << ((UINT8) InitializerOp->Asl.Value.Integer));
            }

            if (i == 1) /* case 1: First IRQ byte */
            {
                RsCreateByteField (InitializerOp, ASL_RESNAME_INTERRUPT,
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


/*******************************************************************************
 *
 * FUNCTION:    RsDoMemory24Descriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory24" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory24Descriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_MEMORY_24_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->M24.DescriptorType  = ACPI_RDESC_TYPE_MEMORY_24;
    Descriptor->M24.Length = 9;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Read/Write type */

            RsSetFlagBits (&Descriptor->M24.Information, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ASL_RESNAME_READWRITETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M24.Information), 0);
            break;

        case 1: /* Min Address */

            Descriptor->M24.AddressMin = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M24.AddressMin));
            break;

        case 2: /* Max Address */

            Descriptor->M24.AddressMax = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M24.AddressMax));
            break;

        case 3: /* Alignment */

            Descriptor->M24.Alignment = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_ALIGNMENT,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M24.Alignment));
            break;

        case 4: /* Length */

            Descriptor->M24.RangeLength = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M24.RangeLength));
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

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoMemory32Descriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory32" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory32Descriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_MEMORY_32_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->M32.DescriptorType  = ACPI_RDESC_TYPE_MEMORY_32;
    Descriptor->M32.Length = 17;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Read/Write type */

            RsSetFlagBits (&Descriptor->M32.Information, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ASL_RESNAME_READWRITETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M32.Information), 0);
            break;

        case 1:  /* Min Address */

            Descriptor->M32.AddressMin = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M32.AddressMin));
            break;

        case 2: /* Max Address */

            Descriptor->M32.AddressMax = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M32.AddressMax));
            break;

        case 3: /* Alignment */

            Descriptor->M32.Alignment = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_ALIGNMENT,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M32.Alignment));
            break;

        case 4: /* Length */

            Descriptor->M32.RangeLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (M32.RangeLength));
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

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoMemory32FixedDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory32Fixed" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory32FixedDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_FIXED_MEMORY_32_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->F32.DescriptorType  = ACPI_RDESC_TYPE_FIXED_MEMORY_32;
    Descriptor->F32.Length = 9;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Read/Write type */

            RsSetFlagBits (&Descriptor->F32.Information, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ASL_RESNAME_READWRITETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (F32.Information), 0);
            break;

        case 1: /* Address */

            Descriptor->F32.BaseAddress = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_BASEADDRESS,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (F32.BaseAddress));
            break;

        case 2: /* Length */

            Descriptor->F32.RangeLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (F32.RangeLength));
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoStartDependentDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "StartDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoStartDependentDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *NextRnode;
    UINT32                  i;
    UINT8                   State;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_START_DEPENDENT_DESC));

    PreviousRnode = Rnode;
    Descriptor = Rnode->Buffer;

    /* Descriptor has priority byte */

    Descriptor->Std.DescriptorType  = ACPI_RDESC_TYPE_START_DEPENDENT |
                                        (ASL_RDESC_ST_DEPEND_SIZE + 0x01);

    /*
     * Process all child initialization nodes
     */
    State = ACPI_RSTATE_START_DEPENDENT;
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Compatibility Priority */

            if ((UINT8) InitializerOp->Asl.Value.Integer > 2)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_PRIORITY, InitializerOp, NULL);
            }

            RsSetFlagBits (&Descriptor->Std.Flags, InitializerOp, 0, 0);
            break;

        case 1: /* Performance/Robustness Priority */

            if ((UINT8) InitializerOp->Asl.Value.Integer > 2)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_PERFORMANCE, InitializerOp, NULL);
            }

            RsSetFlagBits (&Descriptor->Std.Flags, InitializerOp, 2, 0);
            break;

        default:
            NextRnode = RsDoOneResourceDescriptor  (InitializerOp, CurrentByteOffset, &State);

            /*
             * Update current byte offset to indicate the number of bytes from the
             * start of the buffer.  Buffer can include multiple descriptors, we
             * must keep track of the offset of not only each descriptor, but each
             * element (field) within each descriptor as well.
             */

            CurrentByteOffset += RsLinkDescriptorChain (&PreviousRnode, NextRnode);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoStartDependentNoPriDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "StartDependentNoPri" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoStartDependentNoPriDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *NextRnode;
    UINT8                   State;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_START_DEPENDENT_NOPRIO_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Std.DescriptorType  = ACPI_RDESC_TYPE_START_DEPENDENT |
                                        ASL_RDESC_ST_DEPEND_SIZE;
    PreviousRnode = Rnode;

    /*
     * Process all child initialization nodes
     */
    State = ACPI_RSTATE_START_DEPENDENT;
    while (InitializerOp)
    {
        NextRnode = RsDoOneResourceDescriptor  (InitializerOp, CurrentByteOffset, &State);

        /*
         * Update current byte offset to indicate the number of bytes from the
         * start of the buffer.  Buffer can include multiple descriptors, we
         * must keep track of the offset of not only each descriptor, but each
         * element (field) within each descriptor as well.
         */

        CurrentByteOffset += RsLinkDescriptorChain (&PreviousRnode, NextRnode);

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoVendorSmallDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "VendorShort" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoVendorSmallDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_SMALL_VENDOR_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Smv.DescriptorType  = ACPI_RDESC_TYPE_SMALL_VENDOR;

    /*
     * Process all child initialization nodes
     */
    InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    for (i = 0; (InitializerOp && (i < 7)); i++)
    {
        Descriptor->Smv.VendorDefined[i] = (UINT8) InitializerOp->Asl.Value.Integer;
        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Adjust the Rnode buffer size, so correct number of bytes are emitted */

    Rnode->BufferLength -= (7 - i);

    /* Set the length in the Type Tag */

    Descriptor->Smv.DescriptorType |= (UINT8) i;
    return (Rnode);
}


