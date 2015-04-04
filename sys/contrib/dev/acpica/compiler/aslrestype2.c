/******************************************************************************
 *
 * Module Name: aslrestype2 - Miscellaneous Large resource descriptors
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/amlcode.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype2")

/*
 * This module contains miscellaneous large resource descriptors:
 *
 * Register
 * Interrupt
 * VendorLong
 */

/*******************************************************************************
 *
 * FUNCTION:    RsDoGeneralRegisterDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "Register" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoGeneralRegisterDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  CurrentByteOffset;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    CurrentByteOffset = Info->CurrentByteOffset;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_GENERIC_REGISTER));

    Descriptor = Rnode->Buffer;
    Descriptor->GenericReg.DescriptorType = ACPI_RESOURCE_NAME_GENERIC_REGISTER;
    Descriptor->GenericReg.ResourceLength = 12;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Address space */

            Descriptor->GenericReg.AddressSpaceId = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ADDRESSSPACE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (GenericReg.AddressSpaceId));
           break;

        case 1: /* Register Bit Width */

            Descriptor->GenericReg.BitWidth = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_REGISTERBITWIDTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (GenericReg.BitWidth));
            break;

        case 2: /* Register Bit Offset */

            Descriptor->GenericReg.BitOffset = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_REGISTERBITOFFSET,
                CurrentByteOffset + ASL_RESDESC_OFFSET (GenericReg.BitOffset));
            break;

        case 3: /* Register Address */

            Descriptor->GenericReg.Address = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_ADDRESS,
                CurrentByteOffset + ASL_RESDESC_OFFSET (GenericReg.Address));
            break;

        case 4: /* Access Size (ACPI 3.0) */

            Descriptor->GenericReg.AccessSize = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ACCESSSIZE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (GenericReg.AccessSize));

            if (Descriptor->GenericReg.AccessSize > AML_FIELD_ACCESS_QWORD)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_ACCESS_SIZE,
                    InitializerOp, NULL);
            }
            break;

        case 5: /* ResourceTag (ACPI 3.0b) */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
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
 * FUNCTION:    RsDoInterruptDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "Interrupt" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoInterruptDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    AML_RESOURCE            *Rover = NULL;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  CurrentByteOffset;
    UINT32                  i;
    BOOLEAN                 HasResSourceIndex = FALSE;
    UINT8                   ResSourceIndex = 0;
    UINT8                   *ResSourceString = NULL;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    CurrentByteOffset = Info->CurrentByteOffset;
    StringLength = RsGetStringDataLength (InitializerOp);

    /* Count the interrupt numbers */

    for (i = 0; InitializerOp; i++)
    {
        InitializerOp = ASL_GET_PEER_NODE (InitializerOp);

        if (i <= 6)
        {
            if (i == 3 &&
                InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /*
                 * ResourceSourceIndex was specified, always make room for
                 * it, even if the ResourceSource was omitted.
                 */
                OptionIndex++;
            }

            continue;
        }

        OptionIndex += 4;
    }

    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_EXTENDED_IRQ) +
        1 + OptionIndex + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->ExtendedIrq.DescriptorType  = ACPI_RESOURCE_NAME_EXTENDED_IRQ;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->ExtendedIrq.ResourceLength  = 2;  /* Flags and table length byte */
    Descriptor->ExtendedIrq.InterruptCount  = 0;

    Rover = ACPI_CAST_PTR (AML_RESOURCE,
                (&(Descriptor->ExtendedIrq.Interrupts[0])));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage (Default: consumer (1) */

            RsSetFlagBits (&Descriptor->ExtendedIrq.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* Interrupt Type (or Mode - edge/level) */

            RsSetFlagBits (&Descriptor->ExtendedIrq.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtendedIrq.Flags), 1);
            break;

        case 2: /* Interrupt Level (or Polarity - Active high/low) */

            RsSetFlagBits (&Descriptor->ExtendedIrq.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTLEVEL,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtendedIrq.Flags), 2);
            break;

        case 3: /* Share Type - Default: exclusive (0) */

            RsSetFlagBits (&Descriptor->ExtendedIrq.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTSHARE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtendedIrq.Flags), 3);
            break;

        case 4: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                HasResSourceIndex = TRUE;
                ResSourceIndex = (UINT8) InitializerOp->Asl.Value.Integer;
            }
            break;

        case 5: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    ResSourceString = (UINT8 *) InitializerOp->Asl.Value.String;
                }

                /* ResourceSourceIndex must also be valid */

                if (!HasResSourceIndex)
                {
                    AslError (ASL_ERROR, ASL_MSG_RESOURCE_INDEX,
                        InitializerOp, NULL);
                }
            }

#if 0
            /*
             * Not a valid ResourceSource, ResourceSourceIndex must also
             * be invalid
             */
            else if (HasResSourceIndex)
            {
                AslError (ASL_ERROR, ASL_MSG_RESOURCE_SOURCE,
                    InitializerOp, NULL);
            }
#endif
            break;

        case 6: /* ResourceTag */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
            break;

        default:
            /*
             * Interrupt Numbers come through here, repeatedly
             */

            /* Maximum 255 interrupts allowed for this descriptor */

            if (Descriptor->ExtendedIrq.InterruptCount == 255)
            {
                AslError (ASL_ERROR, ASL_MSG_EX_INTERRUPT_LIST,
                    InitializerOp, NULL);
                return (Rnode);
            }

            /* Each interrupt number must be a 32-bit value */

            if (InitializerOp->Asl.Value.Integer > ACPI_UINT32_MAX)
            {
                AslError (ASL_ERROR, ASL_MSG_EX_INTERRUPT_NUMBER,
                    InitializerOp, NULL);
            }

            /* Save the integer and move pointer to the next one */

            Rover->DwordItem = (UINT32) InitializerOp->Asl.Value.Integer;
            Rover = ACPI_ADD_PTR (AML_RESOURCE, &(Rover->DwordItem), 4);
            Descriptor->ExtendedIrq.InterruptCount++;
            Descriptor->ExtendedIrq.ResourceLength += 4;

            /* Case 7: First interrupt number in list */

            if (i == 7)
            {
                if (InitializerOp->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
                {
                    /* Must be at least one interrupt */

                    AslError (ASL_ERROR, ASL_MSG_EX_INTERRUPT_LIST_MIN,
                        InitializerOp, NULL);
                }

                /* Check now for duplicates in list */

                RsCheckListForDuplicates (InitializerOp);

                /* Create a named field at the start of the list */

                RsCreateDwordField (InitializerOp, ACPI_RESTAG_INTERRUPT,
                    CurrentByteOffset +
                    ASL_RESDESC_OFFSET (ExtendedIrq.Interrupts[0]));
            }
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }


    /* Add optional ResSourceIndex if present */

    if (HasResSourceIndex)
    {
        Rover->ByteItem = ResSourceIndex;
        Rover = ACPI_ADD_PTR (AML_RESOURCE, &(Rover->ByteItem), 1);
        Descriptor->ExtendedIrq.ResourceLength += 1;
    }

    /* Add optional ResSource string if present */

    if (StringLength && ResSourceString)
    {

        strcpy ((char *) Rover, (char *) ResSourceString);
        Rover = ACPI_ADD_PTR (
                    AML_RESOURCE, &(Rover->ByteItem), StringLength);

        Descriptor->ExtendedIrq.ResourceLength = (UINT16)
            (Descriptor->ExtendedIrq.ResourceLength + StringLength);
    }

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (ExtendedIrq.Interrupts[0]) -
                           ASL_RESDESC_OFFSET (ExtendedIrq.DescriptorType))
                           + OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoVendorLargeDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "VendorLong" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoVendorLargeDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *VendorData;
    UINT32                  i;


    /* Count the number of data bytes */

    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);

    for (i = 0; InitializerOp; i++)
    {
        if (InitializerOp->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
        {
            break;
        }
        InitializerOp = InitializerOp->Asl.Next;
    }

    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_VENDOR_LARGE) + i);

    Descriptor = Rnode->Buffer;
    Descriptor->VendorLarge.DescriptorType  = ACPI_RESOURCE_NAME_VENDOR_LARGE;
    Descriptor->VendorLarge.ResourceLength = (UINT16) i;

    /* Point to end-of-descriptor for vendor data */

    VendorData = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_LARGE_HEADER);

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        if (InitializerOp->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
        {
            break;
        }

        VendorData[i] = (UINT8) InitializerOp->Asl.Value.Integer;
        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}
