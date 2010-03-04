
/******************************************************************************
 *
 * Module Name: aslrestype2 - Long (type2) resource templates and descriptors
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype2")

/* Local prototypes */

static UINT16
RsGetStringDataLength (
    ACPI_PARSE_OBJECT       *InitializerOp);


/*******************************************************************************
 *
 * FUNCTION:    RsGetStringDataLength
 *
 * PARAMETERS:  InitializerOp     - Start of a subtree of init nodes
 *
 * RETURN:      Valid string length if a string node is found (otherwise 0)
 *
 * DESCRIPTION: In a list of peer nodes, find the first one that contains a
 *              string and return the length of the string.
 *
 ******************************************************************************/

static UINT16
RsGetStringDataLength (
    ACPI_PARSE_OBJECT       *InitializerOp)
{

    while (InitializerOp)
    {
        if (InitializerOp->Asl.ParseOpcode == PARSEOP_STRING_LITERAL)
        {
            return ((UINT16) (strlen (InitializerOp->Asl.Value.String) + 1));
        }
        InitializerOp = ASL_GET_PEER_NODE (InitializerOp);
    }

    return 0;
}


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
            RsCreateBitField (InitializerOp, ACPI_RESTAG_RANGETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 0);
            break;

        case 5: /* Address Granularity */

            Descriptor->Address32.Granularity =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Granularity));
            break;

        case 6: /* Address Min */

            Descriptor->Address32.Minimum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Minimum));
            break;

        case 7: /* Address Max */

            Descriptor->Address32.Maximum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Maximum));
            break;

        case 8: /* Translation Offset */

            Descriptor->Address32.TranslationOffset =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->Address32.AddressLength =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.AddressLength));
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
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MEMTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 1);
            break;

        case 5: /* Read/Write Type */

            RsSetFlagBits (&Descriptor->Address32.SpecificFlags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 0);
            break;

        case 6: /* Address Granularity */

            Descriptor->Address32.Granularity =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Granularity));
            break;

        case 7: /* Min Address */

            Descriptor->Address32.Minimum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Minimum));
            break;

        case 8: /* Max Address */

            Descriptor->Address32.Maximum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Maximum));
            break;

        case 9: /* Translation Offset */

            Descriptor->Address32.TranslationOffset =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Address32.AddressLength =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.AddressLength));
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
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MEMATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.SpecificFlags), 3);
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
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Granularity));
            break;

        case 7: /* Min Address */

            Descriptor->Address32.Minimum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Minimum));
            break;

        case 8: /* Max Address */

            Descriptor->Address32.Maximum =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.Maximum));
            break;

        case 9: /* Translation Offset */

            Descriptor->Address32.TranslationOffset =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Address32.AddressLength =
                (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address32.AddressLength));
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

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS32) +
                            OptionIndex + StringLength;
    return (Rnode);
}


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
           break;

        case 6: /* Address Min */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            break;

        case 7: /* Address Max */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
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
            break;

        case 7: /* Min Address */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            break;

        case 8: /* Max Address */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
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
            break;

        case 7: /* Min Address */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            break;

        case 8: /* Max Address */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
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

    Rnode->BufferLength = sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoQwordIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "QwordIO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoQwordIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address64.DescriptorType  = ACPI_RESOURCE_NAME_ADDRESS64;
    Descriptor->Address64.ResourceType    = ACPI_ADDRESS_TYPE_IO_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS64);
    Descriptor->Address64.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->Address64.SpecificFlags, InitializerOp, 0, 3);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_RANGETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.SpecificFlags), 0);
            break;

        case 5: /* Address Granularity */

            Descriptor->Address64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Granularity));
           break;

        case 6: /* Address Min */

            Descriptor->Address64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Minimum));
            break;

        case 7: /* Address Max */

            Descriptor->Address64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Maximum));
            break;

        case 8: /* Translation Offset */

            Descriptor->Address64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->Address64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.AddressLength));
            break;

        case 10: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address64.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 11: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address64.ResourceLength = (UINT16)
                        (Descriptor->Address64.ResourceLength + StringLength);

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

            RsSetFlagBits (&Descriptor->Address64.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.SpecificFlags), 4);
            break;

        case 14: /* Translation Type */

            RsSetFlagBits (&Descriptor->Address64.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TRANSTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS64) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoQwordMemoryDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "QwordMemory" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoQwordMemoryDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address64.DescriptorType  = ACPI_RESOURCE_NAME_ADDRESS64;
    Descriptor->Address64.ResourceType    = ACPI_ADDRESS_TYPE_MEMORY_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS64);
    Descriptor->Address64.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 1);
            break;

        case 2: /* MinType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 2);
            break;

        case 3: /* MaxType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 3);
            break;

        case 4: /* Memory Type */

            RsSetFlagBits (&Descriptor->Address64.SpecificFlags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MEMTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.SpecificFlags), 1);
            break;

        case 5: /* Read/Write Type */

            RsSetFlagBits (&Descriptor->Address64.SpecificFlags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.SpecificFlags), 0);
            break;

        case 6: /* Address Granularity */

            Descriptor->Address64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Granularity));
            break;

        case 7: /* Min Address */

            Descriptor->Address64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Minimum));
            break;

        case 8: /* Max Address */

            Descriptor->Address64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Maximum));
            break;

        case 9: /* Translation Offset */

            Descriptor->Address64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Address64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.AddressLength));
            break;

        case 11: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address64.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 12: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address64.ResourceLength = (UINT16)
                        (Descriptor->Address64.ResourceLength + StringLength);

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

            RsSetFlagBits (&Descriptor->Address64.SpecificFlags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MEMATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.SpecificFlags), 3);
            break;

        case 15: /* Type */

            RsSetFlagBits (&Descriptor->Address64.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS64) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoQwordSpaceDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "QwordSpace" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoQwordSpaceDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address64.DescriptorType = ACPI_RESOURCE_NAME_ADDRESS64;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS64);
    Descriptor->Address64.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            Descriptor->Address64.ResourceType =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 1: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 0, 1);
            break;

        case 2: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 1);
            break;

        case 3: /* MinType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 2);
            break;

        case 4: /* MaxType */

            RsSetFlagBits (&Descriptor->Address64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Flags), 3);
            break;

        case 5: /* Type-Specific flags */

            Descriptor->Address64.SpecificFlags =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 6: /* Address Granularity */

            Descriptor->Address64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Granularity));
            break;

        case 7: /* Min Address */

            Descriptor->Address64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Minimum));
            break;

        case 8: /* Max Address */

            Descriptor->Address64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.Maximum));
            break;

        case 9: /* Translation Offset */

            Descriptor->Address64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Address64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address64.AddressLength));
            break;

        case 11: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address64.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 12: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address64.ResourceLength = (UINT16)
                        (Descriptor->Address64.ResourceLength + StringLength);

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

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS64) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoWordIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "WordIO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoWordIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS16) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address16.DescriptorType  = ACPI_RESOURCE_NAME_ADDRESS16;
    Descriptor->Address16.ResourceType    = ACPI_ADDRESS_TYPE_IO_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS16);
    Descriptor->Address16.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS16) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->Address16.SpecificFlags, InitializerOp, 0, 3);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_RANGETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.SpecificFlags), 0);
            break;

        case 5: /* Address Granularity */

            Descriptor->Address16.Granularity = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Granularity));
            break;

        case 6: /* Address Min */

            Descriptor->Address16.Minimum = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Minimum));
            break;

        case 7: /* Address Max */

            Descriptor->Address16.Maximum = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Maximum));
            break;

        case 8: /* Translation Offset */

            Descriptor->Address16.TranslationOffset = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->Address16.AddressLength = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                 CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.AddressLength));
            break;

        case 10: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address16.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 11: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address16.ResourceLength = (UINT16)
                        (Descriptor->Address16.ResourceLength + StringLength);

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

            RsSetFlagBits (&Descriptor->Address16.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.SpecificFlags), 4);
            break;

        case 14: /* Translation Type */

            RsSetFlagBits (&Descriptor->Address16.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TRANSTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS16) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoWordBusNumberDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "WordBusNumber" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoWordBusNumberDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS16) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address16.DescriptorType  = ACPI_RESOURCE_NAME_ADDRESS16;
    Descriptor->Address16.ResourceType    = ACPI_ADDRESS_TYPE_BUS_NUMBER_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS16);
    Descriptor->Address16.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS16) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 1);
            break;

        case 4: /* Address Granularity */

            Descriptor->Address16.Granularity =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Granularity));
            break;

        case 5: /* Min Address */

            Descriptor->Address16.Minimum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Minimum));
            break;

        case 6: /* Max Address */

            Descriptor->Address16.Maximum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Maximum));
            break;

        case 7: /* Translation Offset */

            Descriptor->Address16.TranslationOffset =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.TranslationOffset));
            break;

        case 8: /* Address Length */

            Descriptor->Address16.AddressLength =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                 CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.AddressLength));
            break;

        case 9: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address16.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 10: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address16.ResourceLength = (UINT16)
                        (Descriptor->Address16.ResourceLength + StringLength);

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

        case 11: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS16) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoWordSpaceDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "WordSpace" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoWordSpaceDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *OptionalFields;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 ResSourceIndex = FALSE;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_ADDRESS16) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Address16.DescriptorType  = ACPI_RESOURCE_NAME_ADDRESS16;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    OptionalFields = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_ADDRESS16);
    Descriptor->Address16.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_ADDRESS16) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            Descriptor->Address16.ResourceType =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 1: /* Resource Usage */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 0, 1);
            break;

        case 2: /* DecodeType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 1);
            break;

        case 3: /* MinType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 2);
            break;

        case 4: /* MaxType */

            RsSetFlagBits (&Descriptor->Address16.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Flags), 3);
            break;

        case 5: /* Type-Specific flags */

            Descriptor->Address16.SpecificFlags =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 6: /* Address Granularity */

            Descriptor->Address16.Granularity =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Granularity));
            break;

        case 7: /* Min Address */

            Descriptor->Address16.Minimum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Minimum));
            break;

        case 8: /* Max Address */

            Descriptor->Address16.Maximum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.Maximum));
            break;

        case 9: /* Translation Offset */

            Descriptor->Address16.TranslationOffset =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Address16.AddressLength =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Address16.AddressLength));
            break;

        case 11: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Address16.ResourceLength++;
                ResSourceIndex = TRUE;
            }
            break;

        case 12: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Address16.ResourceLength = (UINT16)
                        (Descriptor->Address16.ResourceLength + StringLength);

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

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = sizeof (AML_RESOURCE_ADDRESS16) +
                            OptionIndex + StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoInterruptDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "Interrupt" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoInterruptDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    AML_RESOURCE            *Rover = NULL;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 HasResSourceIndex = FALSE;
    UINT8                   ResSourceIndex = 0;
    UINT8                   *ResSourceString = NULL;


    InitializerOp = Op->Asl.Child;
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

    InitializerOp = Op->Asl.Child;
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

            UtAttachNamepathToOwner (Op, InitializerOp);
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

                RsCreateByteField (InitializerOp, ACPI_RESTAG_INTERRUPT,
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
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "VendorLong" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoVendorLargeDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *VendorData;
    UINT32                  i;


    /* Count the number of data bytes */

    InitializerOp = Op->Asl.Child;
    InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);

    for (i = 0; InitializerOp; i++)
    {
        if (InitializerOp->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
        {
            break;
        }
        InitializerOp = InitializerOp->Asl.Next;
    }

    InitializerOp = Op->Asl.Child;
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


/*******************************************************************************
 *
 * FUNCTION:    RsDoGeneralRegisterDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "Register" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoGeneralRegisterDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
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
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ADDRESS,
                CurrentByteOffset + ASL_RESDESC_OFFSET (GenericReg.Address));
            break;

        case 4: /* Access Size (ACPI 3.0) */

            Descriptor->GenericReg.AccessSize = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ACCESSSIZE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (GenericReg.AccessSize));
            break;

        case 5: /* ResourceTag (ACPI 3.0b) */

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


