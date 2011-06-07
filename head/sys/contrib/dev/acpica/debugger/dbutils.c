/*******************************************************************************
 *
 * Module Name: dbutils - AML debugger utilities
 *
 ******************************************************************************/

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


#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/acdisasm.h>


#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbutils")

/* Local prototypes */

#ifdef ACPI_OBSOLETE_FUNCTIONS
ACPI_STATUS
AcpiDbSecondPassParse (
    ACPI_PARSE_OBJECT       *Root);

void
AcpiDbDumpBuffer (
    UINT32                  Address);
#endif

static char                 *Converter = "0123456789ABCDEF";


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbMatchArgument
 *
 * PARAMETERS:  UserArgument            - User command line
 *              Arguments               - Array of commands to match against
 *
 * RETURN:      Index into command array or ACPI_TYPE_NOT_FOUND if not found
 *
 * DESCRIPTION: Search command array for a command match
 *
 ******************************************************************************/

ACPI_OBJECT_TYPE
AcpiDbMatchArgument (
    char                    *UserArgument,
    ARGUMENT_INFO           *Arguments)
{
    UINT32                  i;


    if (!UserArgument || UserArgument[0] == 0)
    {
        return (ACPI_TYPE_NOT_FOUND);
    }

    for (i = 0; Arguments[i].Name; i++)
    {
        if (ACPI_STRSTR (Arguments[i].Name, UserArgument) == Arguments[i].Name)
        {
            return (i);
        }
    }

    /* Argument not recognized */

    return (ACPI_TYPE_NOT_FOUND);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSetOutputDestination
 *
 * PARAMETERS:  OutputFlags         - Current flags word
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set the current destination for debugger output.  Also sets
 *              the debug output level accordingly.
 *
 ******************************************************************************/

void
AcpiDbSetOutputDestination (
    UINT32                  OutputFlags)
{

    AcpiGbl_DbOutputFlags = (UINT8) OutputFlags;

    if ((OutputFlags & ACPI_DB_REDIRECTABLE_OUTPUT) && AcpiGbl_DbOutputToFile)
    {
        AcpiDbgLevel = AcpiGbl_DbDebugLevel;
    }
    else
    {
        AcpiDbgLevel = AcpiGbl_DbConsoleDebugLevel;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDumpExternalObject
 *
 * PARAMETERS:  ObjDesc         - External ACPI object to dump
 *              Level           - Nesting level.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the contents of an ACPI external object
 *
 ******************************************************************************/

void
AcpiDbDumpExternalObject (
    ACPI_OBJECT             *ObjDesc,
    UINT32                  Level)
{
    UINT32                  i;


    if (!ObjDesc)
    {
        AcpiOsPrintf ("[Null Object]\n");
        return;
    }

    for (i = 0; i < Level; i++)
    {
        AcpiOsPrintf ("  ");
    }

    switch (ObjDesc->Type)
    {
    case ACPI_TYPE_ANY:

        AcpiOsPrintf ("[Null Object] (Type=0)\n");
        break;


    case ACPI_TYPE_INTEGER:

        AcpiOsPrintf ("[Integer] = %8.8X%8.8X\n",
                    ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
        break;


    case ACPI_TYPE_STRING:

        AcpiOsPrintf ("[String] Length %.2X = ", ObjDesc->String.Length);
        for (i = 0; i < ObjDesc->String.Length; i++)
        {
            AcpiOsPrintf ("%c", ObjDesc->String.Pointer[i]);
        }
        AcpiOsPrintf ("\n");
        break;


    case ACPI_TYPE_BUFFER:

        AcpiOsPrintf ("[Buffer] Length %.2X = ", ObjDesc->Buffer.Length);
        if (ObjDesc->Buffer.Length)
        {
            if (ObjDesc->Buffer.Length > 16)
            {
                AcpiOsPrintf ("\n");
            }
            AcpiUtDumpBuffer (ACPI_CAST_PTR (UINT8, ObjDesc->Buffer.Pointer),
                    ObjDesc->Buffer.Length, DB_DWORD_DISPLAY, _COMPONENT);
        }
        else
        {
            AcpiOsPrintf ("\n");
        }
        break;


    case ACPI_TYPE_PACKAGE:

        AcpiOsPrintf ("[Package] Contains %u Elements:\n",
                ObjDesc->Package.Count);

        for (i = 0; i < ObjDesc->Package.Count; i++)
        {
            AcpiDbDumpExternalObject (&ObjDesc->Package.Elements[i], Level+1);
        }
        break;


    case ACPI_TYPE_LOCAL_REFERENCE:

        AcpiOsPrintf ("[Object Reference] = ");
        AcpiDmDisplayInternalObject (ObjDesc->Reference.Handle, NULL);
        break;


    case ACPI_TYPE_PROCESSOR:

        AcpiOsPrintf ("[Processor]\n");
        break;


    case ACPI_TYPE_POWER:

        AcpiOsPrintf ("[Power Resource]\n");
        break;


    default:

        AcpiOsPrintf ("[Unknown Type] %X\n", ObjDesc->Type);
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbPrepNamestring
 *
 * PARAMETERS:  Name            - String to prepare
 *
 * RETURN:      None
 *
 * DESCRIPTION: Translate all forward slashes and dots to backslashes.
 *
 ******************************************************************************/

void
AcpiDbPrepNamestring (
    char                    *Name)
{

    if (!Name)
    {
        return;
    }

    AcpiUtStrupr (Name);

    /* Convert a leading forward slash to a backslash */

    if (*Name == '/')
    {
        *Name = '\\';
    }

    /* Ignore a leading backslash, this is the root prefix */

    if (*Name == '\\')
    {
        Name++;
    }

    /* Convert all slash path separators to dots */

    while (*Name)
    {
        if ((*Name == '/') ||
            (*Name == '\\'))
        {
            *Name = '.';
        }

        Name++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbLocalNsLookup
 *
 * PARAMETERS:  Name            - Name to lookup
 *
 * RETURN:      Pointer to a namespace node, null on failure
 *
 * DESCRIPTION: Lookup a name in the ACPI namespace
 *
 * Note: Currently begins search from the root.  Could be enhanced to use
 * the current prefix (scope) node as the search beginning point.
 *
 ******************************************************************************/

ACPI_NAMESPACE_NODE *
AcpiDbLocalNsLookup (
    char                    *Name)
{
    char                    *InternalPath;
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node = NULL;


    AcpiDbPrepNamestring (Name);

    /* Build an internal namestring */

    Status = AcpiNsInternalizeName (Name, &InternalPath);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Invalid namestring: %s\n", Name);
        return (NULL);
    }

    /*
     * Lookup the name.
     * (Uses root node as the search starting point)
     */
    Status = AcpiNsLookup (NULL, InternalPath, ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
                    ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE, NULL, &Node);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not locate name: %s, %s\n",
                Name, AcpiFormatException (Status));
    }

    ACPI_FREE (InternalPath);
    return (Node);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbUInt32ToHexString
 *
 * PARAMETERS:  Value           - The value to be converted to string
 *              Buffer          - Buffer for result (not less than 11 bytes)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert the unsigned 32-bit value to the hexadecimal image
 *
 * NOTE: It is the caller's responsibility to ensure that the length of buffer
 *       is sufficient.
 *
 ******************************************************************************/

void
AcpiDbUInt32ToHexString (
    UINT32                  Value,
    char                    *Buffer)
{
    int                     i;


    if (Value == 0)
    {
        ACPI_STRCPY (Buffer, "0");
        return;
    }

    Buffer[8] = '\0';

    for (i = 7; i >= 0; i--)
    {
        Buffer[i] = Converter [Value & 0x0F];
        Value = Value >> 4;
    }
}


#ifdef ACPI_OBSOLETE_FUNCTIONS
/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSecondPassParse
 *
 * PARAMETERS:  Root            - Root of the parse tree
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Second pass parse of the ACPI tables.  We need to wait until
 *              second pass to parse the control methods
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbSecondPassParse (
    ACPI_PARSE_OBJECT       *Root)
{
    ACPI_PARSE_OBJECT       *Op = Root;
    ACPI_PARSE_OBJECT       *Method;
    ACPI_PARSE_OBJECT       *SearchOp;
    ACPI_PARSE_OBJECT       *StartOp;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  BaseAmlOffset;
    ACPI_WALK_STATE         *WalkState;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("Pass two parse ....\n");

    while (Op)
    {
        if (Op->Common.AmlOpcode == AML_METHOD_OP)
        {
            Method = Op;

            /* Create a new walk state for the parse */

            WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
            if (!WalkState)
            {
                return (AE_NO_MEMORY);
            }

            /* Init the Walk State */

            WalkState->ParserState.Aml          =
            WalkState->ParserState.AmlStart     = Method->Named.Data;
            WalkState->ParserState.AmlEnd       =
            WalkState->ParserState.PkgEnd       = Method->Named.Data +
                                                  Method->Named.Length;
            WalkState->ParserState.StartScope   = Op;

            WalkState->DescendingCallback       = AcpiDsLoad1BeginOp;
            WalkState->AscendingCallback        = AcpiDsLoad1EndOp;

            /* Perform the AML parse */

            Status = AcpiPsParseAml (WalkState);

            BaseAmlOffset = (Method->Common.Value.Arg)->Common.AmlOffset + 1;
            StartOp = (Method->Common.Value.Arg)->Common.Next;
            SearchOp = StartOp;

            while (SearchOp)
            {
                SearchOp->Common.AmlOffset += BaseAmlOffset;
                SearchOp = AcpiPsGetDepthNext (StartOp, SearchOp);
            }
        }

        if (Op->Common.AmlOpcode == AML_REGION_OP)
        {
            /* TBD: [Investigate] this isn't quite the right thing to do! */
            /*
             *
             * Method = (ACPI_DEFERRED_OP *) Op;
             * Status = AcpiPsParseAml (Op, Method->Body, Method->BodyLength);
             */
        }

        if (ACPI_FAILURE (Status))
        {
            break;
        }

        Op = AcpiPsGetDepthNext (Root, Op);
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDumpBuffer
 *
 * PARAMETERS:  Address             - Pointer to the buffer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a portion of a buffer
 *
 ******************************************************************************/

void
AcpiDbDumpBuffer (
    UINT32                  Address)
{

    AcpiOsPrintf ("\nLocation %X:\n", Address);

    AcpiDbgLevel |= ACPI_LV_TABLES;
    AcpiUtDumpBuffer (ACPI_TO_POINTER (Address), 64, DB_BYTE_DISPLAY,
            ACPI_UINT32_MAX);
}
#endif

#endif /* ACPI_DEBUGGER */


