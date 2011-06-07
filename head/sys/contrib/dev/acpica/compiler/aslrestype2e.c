
/******************************************************************************
 *
 * Module Name: aslrestype2e - Large Extended address resource descriptors
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype2e")

/*
 * This module contains the Extended (64-bit) address space descriptors:
 *
 * ExtendedIO
 * ExtendedMemory
 * ExtendedSpace
 */

/*******************************************************************************
 *
 * FUNCTION:    RsDoExtendedIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "ExtendedIO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoExtendedIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *GranOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->ExtAddress64.DescriptorType  = ACPI_RESOURCE_NAME_EXTENDED_ADDRESS64;
    Descriptor->ExtAddress64.ResourceType    = ACPI_ADDRESS_TYPE_IO_RANGE;
    Descriptor->ExtAddress64.RevisionID      = AML_RESOURCE_EXTENDED_ADDRESS_REVISION;

    Descriptor->ExtAddress64.ResourceLength  = (UINT16)
        (sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 0, 3);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_RANGETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 0);
            break;

        case 5: /* Address Granularity */

            Descriptor->ExtAddress64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Granularity));
            GranOp = InitializerOp;
           break;

        case 6: /* Address Min */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            MinOp = InitializerOp;
            break;

        case 7: /* Address Max */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
            MaxOp = InitializerOp;
            break;

        case 8: /* Translation Offset */

            Descriptor->ExtAddress64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->ExtAddress64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 10: /* Type-Specific Attributes */

            Descriptor->ExtAddress64.TypeSpecific = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TYPESPECIFICATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TypeSpecific));
            break;

        case 11: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        case 12: /* Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 4);
            break;

        case 13: /* Translation Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TRANSTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        Descriptor->ExtAddress64.Minimum,
        Descriptor->ExtAddress64.Maximum,
        Descriptor->ExtAddress64.AddressLength,
        Descriptor->ExtAddress64.Granularity,
        Descriptor->ExtAddress64.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Op);

    Rnode->BufferLength = sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoExtendedMemoryDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "ExtendedMemory" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoExtendedMemoryDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *GranOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->ExtAddress64.DescriptorType  = ACPI_RESOURCE_NAME_EXTENDED_ADDRESS64;
    Descriptor->ExtAddress64.ResourceType    = ACPI_ADDRESS_TYPE_MEMORY_RANGE;
    Descriptor->ExtAddress64.RevisionID      = AML_RESOURCE_EXTENDED_ADDRESS_REVISION;

    Descriptor->ExtAddress64.ResourceLength  = (UINT16)
        (sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* DecodeType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 1);
            break;

        case 2: /* MinType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 2);
            break;

        case 3: /* MaxType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 3);
            break;

        case 4: /* Memory Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MEMTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 1);
            break;

        case 5: /* Read/Write Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 0);
            break;

        case 6: /* Address Granularity */

            Descriptor->ExtAddress64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Granularity));
            GranOp = InitializerOp;
            break;

        case 7: /* Min Address */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            MinOp = InitializerOp;
            break;

        case 8: /* Max Address */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
            MaxOp = InitializerOp;
            break;

        case 9: /* Translation Offset */

            Descriptor->ExtAddress64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->ExtAddress64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 11: /* Type-Specific Attributes */

            Descriptor->ExtAddress64.TypeSpecific = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TYPESPECIFICATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TypeSpecific));
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;


        case 13: /* Address Range */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MEMATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 3);
            break;

        case 14: /* Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        Descriptor->ExtAddress64.Minimum,
        Descriptor->ExtAddress64.Maximum,
        Descriptor->ExtAddress64.AddressLength,
        Descriptor->ExtAddress64.Granularity,
        Descriptor->ExtAddress64.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Op);

    Rnode->BufferLength = sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoExtendedSpaceDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "ExtendedSpace" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoExtendedSpaceDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *GranOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->ExtAddress64.DescriptorType  = ACPI_RESOURCE_NAME_EXTENDED_ADDRESS64;
    Descriptor->ExtAddress64.RevisionID      = AML_RESOURCE_EXTENDED_ADDRESS_REVISION;

    Descriptor->ExtAddress64.ResourceLength  = (UINT16)
        (sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            Descriptor->ExtAddress64.ResourceType =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 1: /* Resource Usage */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 0, 1);
            break;

        case 2: /* DecodeType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 1);
            break;

        case 3: /* MinType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 2);
            break;

        case 4: /* MaxType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 3);
            break;

        case 5: /* Type-Specific flags */

            Descriptor->ExtAddress64.SpecificFlags =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 6: /* Address Granularity */

            Descriptor->ExtAddress64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Granularity));
            GranOp = InitializerOp;
            break;

        case 7: /* Min Address */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            MinOp = InitializerOp;
            break;

        case 8: /* Max Address */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
            MaxOp = InitializerOp;
            break;

        case 9: /* Translation Offset */

            Descriptor->ExtAddress64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->ExtAddress64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 11: /* Type-Specific Attributes */

            Descriptor->ExtAddress64.TypeSpecific = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TYPESPECIFICATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TypeSpecific));
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        Descriptor->ExtAddress64.Minimum,
        Descriptor->ExtAddress64.Maximum,
        Descriptor->ExtAddress64.AddressLength,
        Descriptor->ExtAddress64.Granularity,
        Descriptor->ExtAddress64.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Op);

    Rnode->BufferLength = sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + StringLength;
    return (Rnode);
}
