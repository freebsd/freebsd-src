/******************************************************************************
 *
 * Module Name: aslrestype2d - Large DWord address resource descriptors
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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype2d")

/*
 * This module contains the Dword (32-bit) address space descriptors:
 *
 * DwordIO
 * DwordMemory
 * DwordSpace
 */

/*******************************************************************************
 *
 * FUNCTION:    RsDoDwordIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "DwordIO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoDwordIoDescriptor (
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
    UINT32                  OptionIndex = 0;
    UINT8                   *OptionalFields;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS32) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address32.DescriptorType = ACPI_RESOURCE_NAME_ADDRESS32;
    Descriptor->Address32.ResourceType   = ACPI_ADDRESS_TYPE_IO_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS32);
    Descriptor->Address32.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS32) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 0, 3);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_RANGETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 0, 2);
            break;

        case 5: /* Address Granularity */

            Descriptor->Address32.Granularity =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Granularity));
            GranOp = InitializerOp;
            break;

        case 6: /* Address Min */

            Descriptor->Address32.Minimum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Minimum));
            MinOp = InitializerOp;
            break;

        case 7: /* Address Max */

            Descriptor->Address32.Maximum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Maximum));
            MaxOp = InitializerOp;
            break;

        case 8: /* Translation Offset */

            Descriptor->Address32.TranslationOffset =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->Address32.AddressLength =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 10: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Found a valid ResourceSourceIndex */

                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address32.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 11: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    /* Found a valid ResourceSource */

                    Descriptor->Address32.ResourceLength = (UINT16)
                        (Descriptor->Address32.ResourceLength + StringLength);

                    strcpy ((char *)
                        &OptionalFields[OptionIndex],
                        InitializerOp->Asl.Value.String);

                    /* ResourceSourceIndex must also be valid */

                    if (!ResSourceIndex)
                    {
                        AslError (ASL_ERROR, ASL_MSG_RESOURCE_INDEX,
                            InitializerOp, NULL);
                    }
                }
            }

#if 0
            /*
             * Not a valid ResourceSource, ResourceSourceIndex must also
             * be invalid
             */
            else if (ResSourceIndex)
            {
                AslError (ASL_ERROR, ASL_MSG_RESOURCE_SOURCE,
                    InitializerOp, NULL);
            }
#endif
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        case 13: /* Type */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 4);
            break;

        case 14: /* Translation Type */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TRANSTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        (UINT64) Descriptor->Address32.Minimum,
        (UINT64) Descriptor->Address32.Maximum,
        (UINT64) Descriptor->Address32.AddressLength,
        (UINT64) Descriptor->Address32.Granularity,
        Descriptor->Address32.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Op);

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS32) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoDwordMemoryDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "DwordMemory" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoDwordMemoryDescriptor (
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
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS32) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address32.DescriptorType = ACPI_RESOURCE_NAME_ADDRESS32;
    Descriptor->Address32.ResourceType   = ACPI_ADDRESS_TYPE_MEMORY_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS32);
    Descriptor->Address32.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS32) -
         sizeof (AML_RESOURCE_LARGE_HEADER));


    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 1);
            break;

        case 2: /* MinType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 2);
            break;

        case 3: /* MaxType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 3);
            break;

        case 4: /* Memory Type */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 1, 0);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_MEMTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 1, 2);
            break;

        case 5: /* Read/Write Type */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 0);
            break;

        case 6: /* Address Granularity */

            Descriptor->Address32.Granularity =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Granularity));
            GranOp = InitializerOp;
            break;

        case 7: /* Min Address */

            Descriptor->Address32.Minimum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Minimum));
            MinOp = InitializerOp;
            break;

        case 8: /* Max Address */

            Descriptor->Address32.Maximum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Maximum));
            MaxOp = InitializerOp;
            break;

        case 9: /* Translation Offset */

            Descriptor->Address32.TranslationOffset =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Address32.AddressLength =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 11: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address32.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 12: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address32.ResourceLength = (UINT16)
                        (Descriptor->Address32.ResourceLength + StringLength);

                    strcpy ((char *)
                        &OptionalFields[OptionIndex],
                        InitializerOp->Asl.Value.String);

                    /* ResourceSourceIndex must also be valid */

                    if (!ResSourceIndex)
                    {
                        AslError (ASL_ERROR, ASL_MSG_RESOURCE_INDEX,
                            InitializerOp, NULL);
                    }
                }
            }

#if 0
            /*
             * Not a valid ResourceSource, ResourceSourceIndex must also
             * be invalid
             */
            else if (ResSourceIndex)
            {
                AslError (ASL_ERROR, ASL_MSG_RESOURCE_SOURCE,
                    InitializerOp, NULL);
            }
#endif
            break;

        case 13: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;


        case 14: /* Address Range */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 3, 0);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_MEMATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 3, 2);
            break;

        case 15: /* Type */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        (UINT64) Descriptor->Address32.Minimum,
        (UINT64) Descriptor->Address32.Maximum,
        (UINT64) Descriptor->Address32.AddressLength,
        (UINT64) Descriptor->Address32.Granularity,
        Descriptor->Address32.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Op);

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS32) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoDwordSpaceDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "DwordSpace" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoDwordSpaceDescriptor (
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
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS32) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address32.DescriptorType = ACPI_RESOURCE_NAME_ADDRESS32;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS32);
    Descriptor->Address32.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS32) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            Descriptor->Address32.ResourceType =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 1: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 0, 1);
            break;

        case 2: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 1);
            break;

        case 3: /* MinType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 2);
            break;

        case 4: /* MaxType */

            RsSetFlagBits (&Descriptor->Address32.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Flags), 3);
            break;

        case 5: /* Type-Specific flags */

            Descriptor->Address32.SpecificFlags =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 6: /* Address Granularity */

            Descriptor->Address32.Granularity =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Granularity));
            GranOp = InitializerOp;
            break;

        case 7: /* Min Address */

            Descriptor->Address32.Minimum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Minimum));
            MinOp = InitializerOp;
            break;

        case 8: /* Max Address */

            Descriptor->Address32.Maximum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Maximum));
            MaxOp = InitializerOp;
            break;

        case 9: /* Translation Offset */

            Descriptor->Address32.TranslationOffset =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Address32.AddressLength =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 11: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address32.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 12: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address32.ResourceLength = (UINT16)
                        (Descriptor->Address32.ResourceLength + StringLength);

                    strcpy ((char *)
                        &OptionalFields[OptionIndex],
                        InitializerOp->Asl.Value.String);

                    /* ResourceSourceIndex must also be valid */

                    if (!ResSourceIndex)
                    {
                        AslError (ASL_ERROR, ASL_MSG_RESOURCE_INDEX,
                            InitializerOp, NULL);
                    }
                }
            }

#if 0
            /*
             * Not a valid ResourceSource, ResourceSourceIndex must also
             * be invalid
             */
            else if (ResSourceIndex)
            {
                AslError (ASL_ERROR, ASL_MSG_RESOURCE_SOURCE,
                    InitializerOp, NULL);
            }
#endif
            break;

        case 13: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST,
                InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        (UINT64) Descriptor->Address32.Minimum,
        (UINT64) Descriptor->Address32.Maximum,
        (UINT64) Descriptor->Address32.AddressLength,
        (UINT64) Descriptor->Address32.Granularity,
        Descriptor->Address32.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Op);

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS32) +
                            OptionIndex + StringLength;
    return (Rnode);
}
