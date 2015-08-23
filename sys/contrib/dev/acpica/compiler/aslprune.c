/******************************************************************************
 *
 * Module Name: aslprune - Parse tree prune utility
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
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslprune")


/* Local prototypes */

static ACPI_STATUS
PrTreePruneWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static void
PrPrintObjectAtLevel (
    UINT32                  Level,
    const char              *ObjectName);


typedef struct acpi_prune_info
{
    UINT32                  PruneLevel;
    UINT16                  ParseOpcode;
    UINT16                  Count;

} ACPI_PRUNE_INFO;


/*******************************************************************************
 *
 * FUNCTION:    AslPruneParseTree
 *
 * PARAMETERS:  PruneDepth              - Number of levels to prune
 *              Type                    - Prune type (Device, Method, etc.)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prune off one or more levels of the ASL parse tree
 *
 ******************************************************************************/

void
AslPruneParseTree (
    UINT32                  PruneDepth,
    UINT32                  Type)
{
    ACPI_PRUNE_INFO         PruneObj;


    PruneObj.PruneLevel = PruneDepth;
    PruneObj.Count = 0;

    switch (Type)
    {
    case 0:
        PruneObj.ParseOpcode = (UINT16) PARSEOP_DEVICE;
        break;

    case 1:
        PruneObj.ParseOpcode = (UINT16) PARSEOP_METHOD;
        break;

    case 2:
        PruneObj.ParseOpcode = (UINT16) PARSEOP_IF;
        break;

    default:
        AcpiOsPrintf ("Unsupported type: %u\n", Type);
        return;
    }

    AcpiOsPrintf ("Pruning parse tree, from depth %u\n",
        PruneDepth);

    AcpiOsPrintf ("\nRemoving Objects:\n");

    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD,
        PrTreePruneWalk, NULL, ACPI_CAST_PTR (void, &PruneObj));

    AcpiOsPrintf ("\n%u Total Objects Removed\n", PruneObj.Count);
}


/*******************************************************************************
 *
 * FUNCTION:    PrPrintObjectAtLevel
 *
 * PARAMETERS:  Level                   - Current nesting level
 *              ObjectName              - ACPI name for the object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print object name with indent
 *
 ******************************************************************************/

static void
PrPrintObjectAtLevel (
    UINT32                  Level,
    const char              *ObjectName)
{
    UINT32                  i;


    for (i = 0; i < Level; i++)
    {
        AcpiOsPrintf ("  ");
    }

    AcpiOsPrintf ("[%s] at Level [%u]\n", ObjectName, Level);
}


/*******************************************************************************
 *
 * FUNCTION:    PrTreePruneWalk
 *
 * PARAMETERS:  Parse tree walk callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Prune off one or more levels of the ASL parse tree
 *
 * Current objects that can be pruned are: Devices, Methods, and If/Else
 * blocks.
 *
 ******************************************************************************/

static ACPI_STATUS
PrTreePruneWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_PRUNE_INFO         *PruneObj = (ACPI_PRUNE_INFO *) Context;


    /* We only care about objects below the Prune Level threshold */

    if (Level <= PruneObj->PruneLevel)
    {
        return (AE_OK);
    }

    if ((Op->Asl.ParseOpcode != PruneObj->ParseOpcode) &&
       !(Op->Asl.ParseOpcode == PARSEOP_ELSE &&
             PruneObj->ParseOpcode == PARSEOP_IF))
    {
        return (AE_OK);
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        AcpiOsPrintf ("Method");
        PrPrintObjectAtLevel (Level, Op->Asl.Child->Asl.Value.Name);
        Op->Asl.Child->Asl.Next->Asl.Next->Asl.Next->Asl.Next->Asl.Next->Asl.Next = NULL;
        PruneObj->Count++;
        break;

    case PARSEOP_DEVICE:

        AcpiOsPrintf ("Device");
        PrPrintObjectAtLevel (Level, Op->Asl.Child->Asl.Value.Name);
        Op->Asl.Child->Asl.Next = NULL;
        PruneObj->Count++;
        break;

    case PARSEOP_IF:
    case PARSEOP_ELSE:

        if (Op->Asl.ParseOpcode == PARSEOP_ELSE)
        {
            PrPrintObjectAtLevel(Level, "Else");
            Op->Asl.Child = NULL;
        }
        else
        {
            PrPrintObjectAtLevel(Level, "If");
            Op->Asl.Child->Asl.Next = NULL;
        }

        PruneObj->Count++;
        break;

    default:

        break;
    }

    return (AE_OK);
}
