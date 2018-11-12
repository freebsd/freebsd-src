/******************************************************************************
 *
 * Module Name: aslpld - Implementation of ASL ToPLD macro
 *
 *****************************************************************************/

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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/amlcode.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslpld")


/* Local prototypes */

static UINT8 *
OpcEncodePldBuffer (
    ACPI_PLD_INFO           *PldInfo);

static BOOLEAN
OpcFindName (
    const char              **List,
    char                    *Name,
    UINT32                  *Index);


/*******************************************************************************
 *
 * FUNCTION:    OpcDoPld
 *
 * PARAMETERS:  Op                  - Current parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert ToPLD macro to 20-byte buffer
 *
 * The ToPLD parse tree looks like this:
 *
 *      TOPLD
 *          PLD_REVISION
 *              INTEGER
 *          PLD_IGNORECOLOR
 *              INTEGER
 *          ...
 *          etc.
 *
 ******************************************************************************/

void
OpcDoPld (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PLD_INFO           PldInfo;
    UINT8                   *Buffer;
    ACPI_PARSE_OBJECT       *ThisOp;
    ACPI_PARSE_OBJECT       *NewOp;
    UINT16                  ParseOpcode;
    UINT32                  Value;


    if (!Op)
    {
        AslError (ASL_ERROR, ASL_MSG_NOT_EXIST, Op, NULL);
        return;
    }

    if (Op->Asl.ParseOpcode != PARSEOP_TOPLD)
    {
        AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, Op, NULL);
        return;
    }

    memset (&PldInfo, 0, sizeof (ACPI_PLD_INFO));

    /* Traverse the list of PLD Ops (one per PLD field) */

    ThisOp = Op->Asl.Child;
    while (ThisOp)
    {
        /* Get child values */

        ParseOpcode = ThisOp->Asl.Child->Asl.ParseOpcode;
        Value = (UINT32) ThisOp->Asl.Child->Asl.Value.Integer;

        switch (ThisOp->Asl.ParseOpcode)
        {
        case PARSEOP_PLD_REVISION:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 127)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            PldInfo.Revision = (UINT8) Value;
            break;

        case PARSEOP_PLD_IGNORECOLOR:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 1)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            PldInfo.IgnoreColor = (UINT8) Value;
            break;

        case PARSEOP_PLD_RED:
        case PARSEOP_PLD_GREEN:
        case PARSEOP_PLD_BLUE:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (Value > 255)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_RED)
            {
                PldInfo.Red = (UINT8) Value;
            }
            else if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_GREEN)
            {
                PldInfo.Green = (UINT8) Value;
            }
            else /* PARSEOP_PLD_BLUE */
            {
                PldInfo.Blue = (UINT8) Value;
            }
            break;

        case PARSEOP_PLD_WIDTH:
        case PARSEOP_PLD_HEIGHT:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 65535)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_WIDTH)
            {
                PldInfo.Width = (UINT16) Value;
            }
            else /* PARSEOP_PLD_HEIGHT */
            {
                PldInfo.Height = (UINT16) Value;
            }

            break;

        case PARSEOP_PLD_USERVISIBLE:
        case PARSEOP_PLD_DOCK:
        case PARSEOP_PLD_LID:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 1)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_USERVISIBLE)
            {
                PldInfo.UserVisible = (UINT8) Value;
            }
            else if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_DOCK)
            {
                PldInfo.Dock = (UINT8) Value;
            }
            else
            {
                PldInfo.Lid = (UINT8) Value;
            }

            break;

        case PARSEOP_PLD_PANEL:

            if (ParseOpcode == PARSEOP_INTEGER)
            {
                if (Value > 6)
                {
                    AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName (AcpiGbl_PldPanelList,
                    ThisOp->Asl.Child->Asl.Value.String,
                    &Value))
                {
                    AslError (ASL_ERROR, ASL_MSG_INVALID_OPERAND, ThisOp, NULL);
                    break;
                }
            }

            PldInfo.Panel = (UINT8) Value;
            break;

        case PARSEOP_PLD_VERTICALPOSITION:

            if (ParseOpcode == PARSEOP_INTEGER)
            {
                if (Value > 2)
                {
                    AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName (AcpiGbl_PldVerticalPositionList,
                    ThisOp->Asl.Child->Asl.Value.String,
                    &Value))
                {
                    AslError (ASL_ERROR, ASL_MSG_INVALID_OPERAND, ThisOp, NULL);
                    break;
                }
            }

            PldInfo.VerticalPosition = (UINT8) Value;
            break;

        case PARSEOP_PLD_HORIZONTALPOSITION:

            if (ParseOpcode == PARSEOP_INTEGER)
            {
                if (Value > 2)
                {
                    AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName (AcpiGbl_PldHorizontalPositionList,
                    ThisOp->Asl.Child->Asl.Value.String,
                    &Value))
                {
                    AslError (ASL_ERROR, ASL_MSG_INVALID_OPERAND, ThisOp, NULL);
                    break;
                }
            }

            PldInfo.HorizontalPosition = (UINT8) Value;
            break;

        case PARSEOP_PLD_SHAPE:

            if (ParseOpcode == PARSEOP_INTEGER)
            {
                if (Value > 8)
                {
                    AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName (AcpiGbl_PldShapeList,
                    ThisOp->Asl.Child->Asl.Value.String,
                    &Value))
                {
                    AslError (ASL_ERROR, ASL_MSG_INVALID_OPERAND, ThisOp, NULL);
                    break;
                }
            }

            PldInfo.Shape = (UINT8) Value;
            break;

        case PARSEOP_PLD_GROUPORIENTATION:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 1)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            PldInfo.GroupOrientation = (UINT8) Value;
            break;

        case PARSEOP_PLD_GROUPTOKEN:
        case PARSEOP_PLD_GROUPPOSITION:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 255)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_GROUPTOKEN)
            {
                PldInfo.GroupToken = (UINT8) Value;
            }
            else /* PARSEOP_PLD_GROUPPOSITION */
            {
                PldInfo.GroupPosition = (UINT8) Value;
            }

            break;

        case PARSEOP_PLD_BAY:
        case PARSEOP_PLD_EJECTABLE:
        case PARSEOP_PLD_EJECTREQUIRED:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 1)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_BAY)
            {
                PldInfo.Bay = (UINT8) Value;
            }
            else if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_EJECTABLE)
            {
                PldInfo.Ejectable = (UINT8) Value;
            }
            else /* PARSEOP_PLD_EJECTREQUIRED */
            {
                PldInfo.OspmEjectRequired = (UINT8) Value;
            }

            break;

        case PARSEOP_PLD_CABINETNUMBER:
        case PARSEOP_PLD_CARDCAGENUMBER:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 255)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_CABINETNUMBER)
            {
                PldInfo.CabinetNumber = (UINT8) Value;
            }
            else /* PARSEOP_PLD_CARDCAGENUMBER */
            {
                PldInfo.CardCageNumber = (UINT8) Value;
            }

            break;

        case PARSEOP_PLD_REFERENCE:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 1)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            PldInfo.Reference = (UINT8) Value;
            break;

        case PARSEOP_PLD_ROTATION:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 7)
            {
                switch (Value)
                {
                case 45:

                    Value = 1;
                    break;

                case 90:

                    Value = 2;
                    break;

                case 135:

                    Value = 3;
                    break;

                case 180:

                    Value = 4;
                    break;

                case 225:

                    Value = 5;
                    break;

                case 270:

                    Value = 6;
                    break;

                case 315:

                    Value = 7;
                    break;

                default:

                    AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                    break;
                }
            }

            PldInfo.Rotation = (UINT8) Value;
            break;

        case PARSEOP_PLD_ORDER:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 31)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            PldInfo.Order = (UINT8) Value;
            break;

        case PARSEOP_PLD_VERTICALOFFSET:
        case PARSEOP_PLD_HORIZONTALOFFSET:

            if (ParseOpcode != PARSEOP_INTEGER)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
                break;
            }

            if (Value > 65535)
            {
                AslError (ASL_ERROR, ASL_MSG_RANGE, ThisOp, NULL);
                break;
            }

            if (ThisOp->Asl.ParseOpcode == PARSEOP_PLD_VERTICALOFFSET)
            {
                PldInfo.VerticalOffset = (UINT16) Value;
            }
            else /* PARSEOP_PLD_HORIZONTALOFFSET */
            {
                PldInfo.HorizontalOffset = (UINT16) Value;
            }

            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ThisOp, NULL);
            break;
        }

        ThisOp = ThisOp->Asl.Next;
    }

    Buffer = OpcEncodePldBuffer (&PldInfo);

    /* Change Op to a Buffer */

    Op->Asl.ParseOpcode = PARSEOP_BUFFER;
    Op->Common.AmlOpcode = AML_BUFFER_OP;

    /* Disable further optimization */

    Op->Asl.CompileFlags &= ~NODE_COMPILE_TIME_CONST;
    UtSetParseOpName (Op);

    /* Child node is the buffer length */

    NewOp = TrAllocateNode (PARSEOP_INTEGER);

    NewOp->Asl.AmlOpcode = AML_BYTE_OP;
    NewOp->Asl.Value.Integer = 20;
    NewOp->Asl.Parent = Op;

    Op->Asl.Child = NewOp;
    Op = NewOp;

    /* Peer to the child is the raw buffer data */

    NewOp = TrAllocateNode (PARSEOP_RAW_DATA);
    NewOp->Asl.AmlOpcode = AML_RAW_DATA_BUFFER;
    NewOp->Asl.AmlLength = 20;
    NewOp->Asl.Value.String = ACPI_CAST_PTR (char, Buffer);
    NewOp->Asl.Parent = Op->Asl.Parent;

    Op->Asl.Next = NewOp;
}


/*******************************************************************************
 *
 * FUNCTION:    OpcEncodePldBuffer
 *
 * PARAMETERS:  PldInfo             - _PLD buffer struct (Using local struct)
 *
 * RETURN:      Encode _PLD buffer suitable for return value from _PLD
 *
 * DESCRIPTION: Bit-packs a _PLD buffer struct.
 *
 ******************************************************************************/

static UINT8 *
OpcEncodePldBuffer (
    ACPI_PLD_INFO           *PldInfo)
{
    UINT32                  *Buffer;
    UINT32                  Dword;


    Buffer = ACPI_ALLOCATE_ZEROED (ACPI_PLD_BUFFER_SIZE);
    if (!Buffer)
    {
        return (NULL);
    }

    /* First 32 bits */

    Dword = 0;
    ACPI_PLD_SET_REVISION       (&Dword, PldInfo->Revision);
    ACPI_PLD_SET_IGNORE_COLOR   (&Dword, PldInfo->IgnoreColor);
    ACPI_PLD_SET_RED            (&Dword, PldInfo->Red);
    ACPI_PLD_SET_GREEN          (&Dword, PldInfo->Green);
    ACPI_PLD_SET_BLUE           (&Dword, PldInfo->Blue);
    ACPI_MOVE_32_TO_32          (&Buffer[0], &Dword);

    /* Second 32 bits */

    Dword = 0;
    ACPI_PLD_SET_WIDTH          (&Dword, PldInfo->Width);
    ACPI_PLD_SET_HEIGHT         (&Dword, PldInfo->Height);
    ACPI_MOVE_32_TO_32          (&Buffer[1], &Dword);

    /* Third 32 bits */

    Dword = 0;
    ACPI_PLD_SET_USER_VISIBLE   (&Dword, PldInfo->UserVisible);
    ACPI_PLD_SET_DOCK           (&Dword, PldInfo->Dock);
    ACPI_PLD_SET_LID            (&Dword, PldInfo->Lid);
    ACPI_PLD_SET_PANEL          (&Dword, PldInfo->Panel);
    ACPI_PLD_SET_VERTICAL       (&Dword, PldInfo->VerticalPosition);
    ACPI_PLD_SET_HORIZONTAL     (&Dword, PldInfo->HorizontalPosition);
    ACPI_PLD_SET_SHAPE          (&Dword, PldInfo->Shape);
    ACPI_PLD_SET_ORIENTATION    (&Dword, PldInfo->GroupOrientation);
    ACPI_PLD_SET_TOKEN          (&Dword, PldInfo->GroupToken);
    ACPI_PLD_SET_POSITION       (&Dword, PldInfo->GroupPosition);
    ACPI_PLD_SET_BAY            (&Dword, PldInfo->Bay);
    ACPI_MOVE_32_TO_32          (&Buffer[2], &Dword);

    /* Fourth 32 bits */

    Dword = 0;
    ACPI_PLD_SET_EJECTABLE      (&Dword, PldInfo->Ejectable);
    ACPI_PLD_SET_OSPM_EJECT     (&Dword, PldInfo->OspmEjectRequired);
    ACPI_PLD_SET_CABINET        (&Dword, PldInfo->CabinetNumber);
    ACPI_PLD_SET_CARD_CAGE      (&Dword, PldInfo->CardCageNumber);
    ACPI_PLD_SET_REFERENCE      (&Dword, PldInfo->Reference);
    ACPI_PLD_SET_ROTATION       (&Dword, PldInfo->Rotation);
    ACPI_PLD_SET_ORDER          (&Dword, PldInfo->Order);
    ACPI_MOVE_32_TO_32          (&Buffer[3], &Dword);

    /* Revision 2 adds an additional DWORD */

    if (PldInfo->Revision >= 2)
    {
        /* Fifth 32 bits */

        Dword = 0;
        ACPI_PLD_SET_VERT_OFFSET    (&Dword, PldInfo->VerticalOffset);
        ACPI_PLD_SET_HORIZ_OFFSET   (&Dword, PldInfo->HorizontalOffset);
        ACPI_MOVE_32_TO_32          (&Buffer[4], &Dword);
    }

    return (ACPI_CAST_PTR (UINT8, Buffer));
}


/*******************************************************************************
 *
 * FUNCTION:    OpcFindName
 *
 * PARAMETERS:  List                - Array of char strings to be searched
 *              Name                - Char string to string for
 *              Index               - Index value to set if found
 *
 * RETURN:      TRUE if any names matched, FALSE otherwise
 *
 * DESCRIPTION: Match PLD name to value in lookup table. Sets Value to
 *              equivalent parameter value.
 *
 ******************************************************************************/

static BOOLEAN
OpcFindName (
    const char              **List,
    char                    *Name,
    UINT32                  *Index)
{
    const char              *NameString;
    UINT32                  i;


    AcpiUtStrupr (Name);

    for (i = 0, NameString = List[0];
            NameString;
            i++, NameString = List[i])
    {
        if (!(strncmp (NameString, Name, strlen (Name))))
        {
            *Index = i;
            return (TRUE);
        }
    }

    return (FALSE);
}
