
/******************************************************************************
 *
 * Module Name: aslrestype2 - Long (type2) resource templates and descriptors
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
        ACPI_MODULE_NAME    ("aslrestype2")


/*******************************************************************************
 *
 * FUNCTION:    RsGetStringDataLength
 *
 * PARAMETERS:  InitializerOp     - Start of a subtree of init nodes
 *
 * RETURN:      Valid string length if a string node is found
 *
 * DESCRIPTION: In a list of peer nodes, find the first one that contains a
 *              string and return the length of the string.
 *
 ******************************************************************************/

UINT32
RsGetStringDataLength (
    ACPI_PARSE_OBJECT       *InitializerOp)
{

    while (InitializerOp)
    {
        if (InitializerOp->Asl.ParseOpcode == PARSEOP_STRING_LITERAL)
        {
            return (strlen (InitializerOp->Asl.Value.String) + 1);
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (sizeof (ASL_DWORD_ADDRESS_DESC) +
                                    StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Das.DescriptorType  = ACPI_RDESC_TYPE_DWORD_ADDRESS_SPACE;
    Descriptor->Das.ResourceType    = ACPI_RESOURCE_TYPE_IO_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->Das.Length = (UINT16) (ASL_RESDESC_OFFSET (Das.OptionalFields[0]) -
                                       ASL_RESDESC_OFFSET (Das.ResourceType));

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MINTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MAXTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DECODE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->Das.SpecificFlags, InitializerOp, 0, 3);
            RsCreateBitField (InitializerOp, ASL_RESNAME_RANGETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.SpecificFlags), 0);
            break;

        case 5: /* Address Granularity */

            Descriptor->Das.Granularity = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_GRANULARITY,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Granularity));
            break;

        case 6: /* Address Min */

            Descriptor->Das.AddressMin = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.AddressMin));
            break;

        case 7: /* Address Max */

            Descriptor->Das.AddressMax = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.AddressMax));
            break;

        case 8: /* Translation Offset */

            Descriptor->Das.TranslationOffset = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_TRANSLATION,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->Das.AddressLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.AddressLength));
            break;

        case 10: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                Descriptor->Das.OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Das.Length++;
            }
            break;

        case 11: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Das.Length = (UINT16) (Descriptor->Das.Length + StringLength);

                    strcpy ((char *) &Descriptor->Das.OptionalFields[OptionIndex],
                            InitializerOp->Asl.Value.String);
                }
            }
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        case 13: /* Type */

            RsSetFlagBits (&Descriptor->Das.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.SpecificFlags), 4);
            break;

        case 14: /* Translation Type */

            RsSetFlagBits (&Descriptor->Das.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TRANSTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (Das.OptionalFields[0]) -
                           ASL_RESDESC_OFFSET (Das.DescriptorType))
                           + OptionIndex + StringLength;
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (sizeof (ASL_DWORD_ADDRESS_DESC) +
                                    StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Das.DescriptorType  = ACPI_RDESC_TYPE_DWORD_ADDRESS_SPACE;
    Descriptor->Das.ResourceType    = ACPI_RESOURCE_TYPE_MEMORY_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->Das.Length = (UINT16) (ASL_RESDESC_OFFSET (Das.OptionalFields[0]) -
                                       ASL_RESDESC_OFFSET (Das.ResourceType));

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* DecodeType */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DECODE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Flags), 1);
            break;

        case 2: /* MinType */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MINTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Flags), 2);
            break;

        case 3: /* MaxType */

            RsSetFlagBits (&Descriptor->Das.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MAXTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Flags), 3);
            break;

        case 4: /* Memory Type */

            RsSetFlagBits (&Descriptor->Das.SpecificFlags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MEMTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.SpecificFlags), 1);
            break;

        case 5: /* Read/Write Type */

            RsSetFlagBits (&Descriptor->Das.SpecificFlags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ASL_RESNAME_READWRITETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.SpecificFlags), 0);
            break;

        case 6: /* Address Granularity */

            Descriptor->Das.Granularity = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_GRANULARITY,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.Granularity));
            break;

        case 7: /* Min Address */

            Descriptor->Das.AddressMin = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.AddressMin));
            break;

        case 8: /* Max Address */

            Descriptor->Das.AddressMax = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.AddressMax));
            break;

        case 9: /* Translation Offset */

            Descriptor->Das.TranslationOffset = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_TRANSLATION,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Das.AddressLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.AddressLength));
            break;

        case 11: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                Descriptor->Das.OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Das.Length++;
            }
            break;

        case 12: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Das.Length = (UINT16) (Descriptor->Das.Length + StringLength);

                    strcpy ((char *) &Descriptor->Das.OptionalFields[OptionIndex],
                            InitializerOp->Asl.Value.String);
                }
            }
            break;

        case 13: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;


        case 14: /* Address Range */

            RsSetFlagBits (&Descriptor->Das.SpecificFlags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MEMATTRIBUTES,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.SpecificFlags), 3);
            break;

        case 15: /* Type */

            RsSetFlagBits (&Descriptor->Das.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Das.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (Das.OptionalFields[0]) -
                           ASL_RESDESC_OFFSET (Das.DescriptorType))
                           + OptionIndex + StringLength;
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (sizeof (ASL_QWORD_ADDRESS_DESC) +
                                    StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Qas.DescriptorType  = ACPI_RDESC_TYPE_QWORD_ADDRESS_SPACE;
    Descriptor->Qas.ResourceType    = ACPI_RESOURCE_TYPE_IO_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->Qas.Length = (UINT16) (ASL_RESDESC_OFFSET (Qas.OptionalFields[0]) -
                                       ASL_RESDESC_OFFSET (Qas.ResourceType));
    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MINTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MAXTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DECODE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->Qas.SpecificFlags, InitializerOp, 0, 3);
            RsCreateBitField (InitializerOp, ASL_RESNAME_RANGETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.SpecificFlags), 0);
            break;

        case 5: /* Address Granularity */

            Descriptor->Qas.Granularity = InitializerOp->Asl.Value.Integer;
             RsCreateByteField (InitializerOp, ASL_RESNAME_GRANULARITY,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Granularity));
           break;

        case 6: /* Address Min */

            Descriptor->Qas.AddressMin = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.AddressMin));
            break;

        case 7: /* Address Max */

            Descriptor->Qas.AddressMax = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.AddressMax));
            break;

        case 8: /* Translation Offset */

            Descriptor->Qas.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_TRANSLATION,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->Qas.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.AddressLength));
            break;

        case 10: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                Descriptor->Qas.OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Qas.Length++;
            }
            break;

        case 11: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Qas.Length = (UINT16) (Descriptor->Qas.Length + StringLength);

                    strcpy ((char *) &Descriptor->Qas.OptionalFields[OptionIndex],
                            InitializerOp->Asl.Value.String);
                }
            }
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        case 13: /* Type */

            RsSetFlagBits (&Descriptor->Qas.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.SpecificFlags), 4);
            break;

        case 14: /* Translation Type */

            RsSetFlagBits (&Descriptor->Qas.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TRANSTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (Qas.OptionalFields[0]) -
                           ASL_RESDESC_OFFSET (Qas.DescriptorType))
                           + OptionIndex + StringLength;
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (sizeof (ASL_QWORD_ADDRESS_DESC) +
                                    StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Qas.DescriptorType  = ACPI_RDESC_TYPE_QWORD_ADDRESS_SPACE;
    Descriptor->Qas.ResourceType    = ACPI_RESOURCE_TYPE_MEMORY_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->Qas.Length = (UINT16) (ASL_RESDESC_OFFSET (Qas.OptionalFields[0]) -
                                       ASL_RESDESC_OFFSET (Qas.ResourceType));
    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* DecodeType */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DECODE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Flags), 1);
            break;

        case 2: /* MinType */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MINTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Flags), 2);
            break;

        case 3: /* MaxType */

            RsSetFlagBits (&Descriptor->Qas.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MAXTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Flags), 3);
            break;

        case 4: /* Memory Type */

            RsSetFlagBits (&Descriptor->Qas.SpecificFlags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MEMTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.SpecificFlags), 1);
            break;

        case 5: /* Read/Write Type */

            RsSetFlagBits (&Descriptor->Qas.SpecificFlags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ASL_RESNAME_READWRITETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.SpecificFlags), 0);
            break;

        case 6: /* Address Granularity */

            Descriptor->Qas.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_GRANULARITY,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.Granularity));
            break;

        case 7: /* Min Address */

            Descriptor->Qas.AddressMin = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.AddressMin));
            break;

        case 8: /* Max Address */

            Descriptor->Qas.AddressMax = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.AddressMax));
            break;

        case 9: /* Translation Offset */

            Descriptor->Qas.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_TRANSLATION,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->Qas.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.AddressLength));
            break;

        case 11: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                Descriptor->Qas.OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Qas.Length++;
            }
            break;

        case 12: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Qas.Length = (UINT16) (Descriptor->Qas.Length + StringLength);

                    strcpy ((char *) &Descriptor->Qas.OptionalFields[OptionIndex],
                            InitializerOp->Asl.Value.String);
                }
            }
            break;

        case 13: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;


        case 14: /* Address Range */

            RsSetFlagBits (&Descriptor->Qas.SpecificFlags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MEMATTRIBUTES,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.SpecificFlags), 3);
            break;

        case 15: /* Type */

            RsSetFlagBits (&Descriptor->Qas.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Qas.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (Qas.OptionalFields[0]) -
                           ASL_RESDESC_OFFSET (Qas.DescriptorType))
                           + OptionIndex + StringLength;
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (sizeof (ASL_WORD_ADDRESS_DESC) +
                                    StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Was.DescriptorType  = ACPI_RDESC_TYPE_WORD_ADDRESS_SPACE;
    Descriptor->Was.ResourceType    = ACPI_RESOURCE_TYPE_IO_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->Was.Length = (UINT16) (ASL_RESDESC_OFFSET (Was.OptionalFields[0]) -
                                       ASL_RESDESC_OFFSET (Was.ResourceType));

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MINTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MAXTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DECODE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->Was.SpecificFlags, InitializerOp, 0, 3);
            RsCreateBitField (InitializerOp, ASL_RESNAME_RANGETYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.SpecificFlags), 0);
            break;

        case 5: /* Address Granularity */

            Descriptor->Was.Granularity = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_GRANULARITY,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Granularity));
            break;

        case 6: /* Address Min */

            Descriptor->Was.AddressMin = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.AddressMin));
            break;

        case 7: /* Address Max */

            Descriptor->Was.AddressMax = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.AddressMax));
            break;

        case 8: /* Translation Offset */

            Descriptor->Was.TranslationOffset = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_TRANSLATION,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->Was.AddressLength = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.AddressLength));
            break;

        case 10: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                Descriptor->Was.OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Was.Length++;
            }
            break;

        case 11: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Was.Length = (UINT16) (Descriptor->Was.Length +StringLength);

                    strcpy ((char *) &Descriptor->Was.OptionalFields[OptionIndex],
                            InitializerOp->Asl.Value.String);
                }
            }
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        case 13: /* Type */

            RsSetFlagBits (&Descriptor->Was.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.SpecificFlags), 4);
            break;

        case 14: /* Translation Type */

            RsSetFlagBits (&Descriptor->Was.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_TRANSTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (Was.OptionalFields[0]) -
                           ASL_RESDESC_OFFSET (Was.DescriptorType))
                           + OptionIndex + StringLength;
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);

    Rnode = RsAllocateResourceNode (sizeof (ASL_WORD_ADDRESS_DESC) +
                                    StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->Was.DescriptorType  = ACPI_RDESC_TYPE_WORD_ADDRESS_SPACE;
    Descriptor->Was.ResourceType    = ACPI_RESOURCE_TYPE_BUS_NUMBER_RANGE;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->Was.Length = (UINT16) (ASL_RESDESC_OFFSET (Was.OptionalFields[0]) -
                                       ASL_RESDESC_OFFSET (Was.ResourceType));

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MINTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_MAXTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->Was.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_DECODE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Flags), 1);
            break;

        case 4: /* Address Granularity */

            Descriptor->Was.Granularity = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_GRANULARITY,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.Granularity));
            break;

        case 5: /* Min Address */

            Descriptor->Was.AddressMin = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MINADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.AddressMin));
            break;

        case 6: /* Max Address */

            Descriptor->Was.AddressMax = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_MAXADDR,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.AddressMax));
            break;

        case 7: /* Translation Offset */

            Descriptor->Was.TranslationOffset = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_TRANSLATION,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.TranslationOffset));
            break;

        case 8: /* Address Length */

            Descriptor->Was.AddressLength = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_LENGTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Was.AddressLength));
            break;

        case 9: /* ResSourceIndex [Optional Field - BYTE] */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                Descriptor->Was.OptionalFields[0] = (UINT8) InitializerOp->Asl.Value.Integer;
                OptionIndex++;
                Descriptor->Was.Length++;
            }
            break;

        case 10: /* ResSource [Optional Field - STRING] */

            if ((InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
                (InitializerOp->Asl.Value.String))
            {
                if (StringLength)
                {
                    Descriptor->Was.Length = (UINT16) (Descriptor->Was.Length + StringLength);

                    strcpy ((char *) &Descriptor->Was.OptionalFields[OptionIndex],
                            InitializerOp->Asl.Value.String);
                }
            }
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

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (Was.OptionalFields[0]) -
                           ASL_RESDESC_OFFSET (Was.DescriptorType))
                           + OptionIndex + StringLength;
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
    ASL_RESOURCE_DESC       *Descriptor;
    ASL_RESOURCE_DESC       *Rover = NULL;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  StringLength = 0;
    UINT32                  OptionIndex = 0;
    UINT32                  i;
    BOOLEAN                 HasResSourceIndex = FALSE;
    UINT8                   ResSourceIndex = 0;
    UINT8                   *ResSourceString = NULL;


    InitializerOp = Op->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);
    if (StringLength)
    {
        /* Make room for the ResourceSourceIndex */

        OptionIndex++;
    }

    /* Count the interrupt numbers */

    for (i = 0; InitializerOp; i++)
    {
        InitializerOp = ASL_GET_PEER_NODE (InitializerOp);
        if (i <= 6)
        {
            continue;
        }

        OptionIndex += 4;
    }

    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_EXTENDED_XRUPT_DESC) +
                                    OptionIndex + StringLength);
    Descriptor = Rnode->Buffer;
    Descriptor->Exx.DescriptorType  = ACPI_RDESC_TYPE_EXTENDED_XRUPT;

    /*
     * Initial descriptor length -- may be enlarged if there are
     * optional fields present
     */
    Descriptor->Exx.Length          = 2;  /* Flags and table length byte */
    Descriptor->Exx.TableLength     = 0;

    Rover = ACPI_CAST_PTR (ASL_RESOURCE_DESC, (&(Descriptor->Exx.InterruptNumber[0])));

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type (Default: consumer (1) */

            RsSetFlagBits (&Descriptor->Exx.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* Interrupt Type (or Mode - edge/level) */

            RsSetFlagBits (&Descriptor->Exx.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_INTERRUPTTYPE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Exx.Flags), 0);
            break;

        case 2: /* Interrupt Level (or Polarity - Active high/low) */

            RsSetFlagBits (&Descriptor->Exx.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_INTERRUPTLEVEL,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Exx.Flags), 2);
            break;

        case 3: /* Share Type - Default: exclusive (0) */

            RsSetFlagBits (&Descriptor->Exx.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ASL_RESNAME_INTERRUPTSHARE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Exx.Flags), 3);
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
            }
            break;

        case 6: /* ResourceTag */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:
            /*
             * Interrupt Numbers come through here, repeatedly.
             * Store the integer and move pointer to the next one.
             */
            Rover->U32Item = (UINT32) InitializerOp->Asl.Value.Integer;
            Rover = ACPI_PTR_ADD (ASL_RESOURCE_DESC, &(Rover->U32Item), 4);

            Descriptor->Exx.TableLength++;
            Descriptor->Exx.Length += 4;

            if (i == 7) /* case 7: First interrupt number */
            {
                RsCreateByteField (InitializerOp, ASL_RESNAME_INTERRUPT,
                                    CurrentByteOffset + ASL_RESDESC_OFFSET (Exx.InterruptNumber[0]));
            }
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /*
     * Add optional ResSourceIndex if present
     */
    if (HasResSourceIndex)
    {
        Rover->U8Item = ResSourceIndex;
        Rover = ACPI_PTR_ADD (ASL_RESOURCE_DESC, &(Rover->U8Item), 1);
        Descriptor->Exx.Length += 1;
    }

    /*
     * Add optional ResSource string if present
     */
    if (StringLength && ResSourceString)
    {

        strcpy ((char *) Rover, (char *) ResSourceString);
        Rover = ACPI_PTR_ADD (ASL_RESOURCE_DESC, &(Rover->U8Item), StringLength);
        Descriptor->Exx.Length = (UINT16) (Descriptor->Exx.Length + StringLength);
    }

    Rnode->BufferLength = (ASL_RESDESC_OFFSET (Exx.InterruptNumber[0]) -
                           ASL_RESDESC_OFFSET (Exx.DescriptorType))
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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    /* Count the number of data bytes */

    InitializerOp = Op->Asl.Child;
    InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);

    for (i = 0; InitializerOp; i++)
    {
        InitializerOp = InitializerOp->Asl.Next;
    }

    InitializerOp = Op->Asl.Child;
    InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    Rnode = RsAllocateResourceNode (sizeof (ASL_LARGE_VENDOR_DESC) + (i - 1));

    Descriptor = Rnode->Buffer;
    Descriptor->Lgv.DescriptorType  = ACPI_RDESC_TYPE_LARGE_VENDOR;
    Descriptor->Lgv.Length = (UINT16) i;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        Descriptor->Lgv.VendorDefined[i] = (UINT8) InitializerOp->Asl.Value.Integer;

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
    ASL_RESOURCE_DESC       *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (ASL_GENERAL_REGISTER_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Grg.DescriptorType  = ACPI_RDESC_TYPE_GENERAL_REGISTER;
    Descriptor->Grg.Length          = 12;

    /*
     * Process all child initialization nodes
     */
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Address space */

            Descriptor->Grg.AddressSpaceId = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_ADDRESSSPACE,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Grg.AddressSpaceId));
           break;

        case 1: /* Register Bit Width */

            Descriptor->Grg.BitWidth = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_REGISTERBITWIDTH,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Grg.BitWidth));
            break;

        case 2: /* Register Bit Offset */

            Descriptor->Grg.BitOffset = (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_REGISTERBITOFFSET,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Grg.BitOffset));
            break;

        case 3: /* Register Address */

            Descriptor->Grg.Address = InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ASL_RESNAME_ADDRESS,
                                CurrentByteOffset + ASL_RESDESC_OFFSET (Grg.Address));
            break;


        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }
    return (Rnode);
}


