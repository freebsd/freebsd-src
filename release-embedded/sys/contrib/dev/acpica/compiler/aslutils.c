/******************************************************************************
 *
 * Module Name: aslutils -- compiler utilities
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
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslutils")


/* Local prototypes */

static void
UtPadNameWithUnderscores (
    char                    *NameSeg,
    char                    *PaddedNameSeg);

static void
UtAttachNameseg (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name);


/*******************************************************************************
 *
 * FUNCTION:    UtDisplaySupportedTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print all supported ACPI table names.
 *
 ******************************************************************************/

#define ACPI_TABLE_HELP_FORMAT  "%8u) %s    %s\n"

void
UtDisplaySupportedTables (
    void)
{
    ACPI_DMTABLE_DATA       *TableData;
    UINT32                  i;


    printf ("\nACPI tables supported by iASL version %8.8X:\n"
        "  (Compiler, Disassembler, Template Generator)\n\n",
        ACPI_CA_VERSION);

    /* Special tables */

    printf ("  Special tables and AML tables:\n");
    printf (ACPI_TABLE_HELP_FORMAT, 1, ACPI_RSDP_NAME, "Root System Description Pointer");
    printf (ACPI_TABLE_HELP_FORMAT, 2, ACPI_SIG_FACS, "Firmware ACPI Control Structure");
    printf (ACPI_TABLE_HELP_FORMAT, 3, ACPI_SIG_DSDT, "Differentiated System Description Table");
    printf (ACPI_TABLE_HELP_FORMAT, 4, ACPI_SIG_SSDT, "Secondary System Description Table");

    /* All data tables with common table header */

    printf ("\n  Standard ACPI data tables:\n");
    for (TableData = AcpiDmTableData, i = 5; TableData->Signature; TableData++, i++)
    {
        printf (ACPI_TABLE_HELP_FORMAT, i, TableData->Signature, TableData->Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplayConstantOpcodes
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print AML opcodes that can be used in constant expressions.
 *
 ******************************************************************************/

void
UtDisplayConstantOpcodes (
    void)
{
    UINT32                  i;


    printf ("Constant expression opcode information\n\n");

    for (i = 0; i < sizeof (AcpiGbl_AmlOpInfo) / sizeof (ACPI_OPCODE_INFO); i++)
    {
        if (AcpiGbl_AmlOpInfo[i].Flags & AML_CONSTANT)
        {
            printf ("%s\n", AcpiGbl_AmlOpInfo[i].Name);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtLocalCalloc
 *
 * PARAMETERS:  Size                - Bytes to be allocated
 *
 * RETURN:      Pointer to the allocated memory. Guaranteed to be valid.
 *
 * DESCRIPTION: Allocate zero-initialized memory. Aborts the compile on an
 *              allocation failure, on the assumption that nothing more can be
 *              accomplished.
 *
 ******************************************************************************/

void *
UtLocalCalloc (
    UINT32                  Size)
{
    void                    *Allocated;


    Allocated = ACPI_ALLOCATE_ZEROED (Size);
    if (!Allocated)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_MEMORY_ALLOCATION,
            Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
            Gbl_InputByteCount, Gbl_CurrentColumn,
            Gbl_Files[ASL_FILE_INPUT].Filename, NULL);

        CmCleanupAndExit ();
        exit (1);
    }

    TotalAllocations++;
    TotalAllocated += Size;
    return (Allocated);
}


/*******************************************************************************
 *
 * FUNCTION:    UtBeginEvent
 *
 * PARAMETERS:  Name                - Ascii name of this event
 *
 * RETURN:      Event number (integer index)
 *
 * DESCRIPTION: Saves the current time with this event
 *
 ******************************************************************************/

UINT8
UtBeginEvent (
    char                    *Name)
{

    if (AslGbl_NextEvent >= ASL_NUM_EVENTS)
    {
        AcpiOsPrintf ("Ran out of compiler event structs!\n");
        return (AslGbl_NextEvent);
    }

    /* Init event with current (start) time */

    AslGbl_Events[AslGbl_NextEvent].StartTime = AcpiOsGetTimer ();
    AslGbl_Events[AslGbl_NextEvent].EventName = Name;
    AslGbl_Events[AslGbl_NextEvent].Valid = TRUE;

    return (AslGbl_NextEvent++);
}


/*******************************************************************************
 *
 * FUNCTION:    UtEndEvent
 *
 * PARAMETERS:  Event               - Event number (integer index)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Saves the current time (end time) with this event
 *
 ******************************************************************************/

void
UtEndEvent (
    UINT8                   Event)
{

    if (Event >= ASL_NUM_EVENTS)
    {
        return;
    }

    /* Insert end time for event */

    AslGbl_Events[Event].EndTime = AcpiOsGetTimer ();
}


/*******************************************************************************
 *
 * FUNCTION:    UtHexCharToValue
 *
 * PARAMETERS:  HexChar             - Hex character in Ascii
 *
 * RETURN:      The binary value of the hex character
 *
 * DESCRIPTION: Perform ascii-to-hex translation
 *
 ******************************************************************************/

UINT8
UtHexCharToValue (
    int                     HexChar)
{

    if (HexChar <= 0x39)
    {
        return ((UINT8) (HexChar - 0x30));
    }

    if (HexChar <= 0x46)
    {
        return ((UINT8) (HexChar - 0x37));
    }

    return ((UINT8) (HexChar - 0x57));
}


/*******************************************************************************
 *
 * FUNCTION:    UtConvertByteToHex
 *
 * PARAMETERS:  RawByte             - Binary data
 *              Buffer              - Pointer to where the hex bytes will be
 *                                    stored
 *
 * RETURN:      Ascii hex byte is stored in Buffer.
 *
 * DESCRIPTION: Perform hex-to-ascii translation. The return data is prefixed
 *              with "0x"
 *
 ******************************************************************************/

void
UtConvertByteToHex (
    UINT8                   RawByte,
    UINT8                   *Buffer)
{

    Buffer[0] = '0';
    Buffer[1] = 'x';

    Buffer[2] = (UINT8) AslHexLookup[(RawByte >> 4) & 0xF];
    Buffer[3] = (UINT8) AslHexLookup[RawByte & 0xF];
}


/*******************************************************************************
 *
 * FUNCTION:    UtConvertByteToAsmHex
 *
 * PARAMETERS:  RawByte             - Binary data
 *              Buffer              - Pointer to where the hex bytes will be
 *                                    stored
 *
 * RETURN:      Ascii hex byte is stored in Buffer.
 *
 * DESCRIPTION: Perform hex-to-ascii translation. The return data is prefixed
 *              with "0x"
 *
 ******************************************************************************/

void
UtConvertByteToAsmHex (
    UINT8                   RawByte,
    UINT8                   *Buffer)
{

    Buffer[0] = '0';
    Buffer[1] = (UINT8) AslHexLookup[(RawByte >> 4) & 0xF];
    Buffer[2] = (UINT8) AslHexLookup[RawByte & 0xF];
    Buffer[3] = 'h';
}


/*******************************************************************************
 *
 * FUNCTION:    DbgPrint
 *
 * PARAMETERS:  Type                - Type of output
 *              Fmt                 - Printf format string
 *              ...                 - variable printf list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Conditional print statement. Prints to stderr only if the
 *              debug flag is set.
 *
 ******************************************************************************/

void
DbgPrint (
    UINT32                  Type,
    char                    *Fmt,
    ...)
{
    va_list                 Args;


    if (!Gbl_DebugFlag)
    {
        return;
    }

    if ((Type == ASL_PARSE_OUTPUT) &&
        (!(AslCompilerdebug)))
    {
        return;
    }

    va_start (Args, Fmt);
    (void) vfprintf (stderr, Fmt, Args);
    va_end (Args);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    UtPrintFormattedName
 *
 * PARAMETERS:  ParseOpcode         - Parser keyword ID
 *              Level               - Indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print the ascii name of the parse opcode.
 *
 ******************************************************************************/

#define TEXT_OFFSET 10

void
UtPrintFormattedName (
    UINT16                  ParseOpcode,
    UINT32                  Level)
{

    if (Level)
    {
        DbgPrint (ASL_TREE_OUTPUT,
            "%*s", (3 * Level), " ");
    }
    DbgPrint (ASL_TREE_OUTPUT,
        " %-20.20s", UtGetOpName (ParseOpcode));

    if (Level < TEXT_OFFSET)
    {
        DbgPrint (ASL_TREE_OUTPUT,
            "%*s", (TEXT_OFFSET - Level) * 3, " ");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtSetParseOpName
 *
 * PARAMETERS:  Op                  - Parse op to be named.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert the ascii name of the parse opcode
 *
 ******************************************************************************/

void
UtSetParseOpName (
    ACPI_PARSE_OBJECT       *Op)
{

    strncpy (Op->Asl.ParseOpName, UtGetOpName (Op->Asl.ParseOpcode),
        ACPI_MAX_PARSEOP_NAME);
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplaySummary
 *
 * PARAMETERS:  FileID              - ID of outpout file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display compilation statistics
 *
 ******************************************************************************/

void
UtDisplaySummary (
    UINT32                  FileId)
{
    UINT32                  i;


    if (FileId != ASL_FILE_STDOUT)
    {
        /* Compiler name and version number */

        FlPrintFile (FileId, "%s version %X%s\n\n",
            ASL_COMPILER_NAME, (UINT32) ACPI_CA_VERSION, ACPI_WIDTH);
    }

    /* Summary of main input and output files */

    if (Gbl_FileType == ASL_INPUT_TYPE_ASCII_DATA)
    {
        FlPrintFile (FileId,
            "%-14s %s - %u lines, %u bytes, %u fields\n",
            "Table Input:",
            Gbl_Files[ASL_FILE_INPUT].Filename, Gbl_CurrentLineNumber,
            Gbl_InputByteCount, Gbl_InputFieldCount);

        if ((Gbl_ExceptionCount[ASL_ERROR] == 0) || (Gbl_IgnoreErrors))
        {
            FlPrintFile (FileId,
                "%-14s %s - %u bytes\n",
                "Binary Output:",
                Gbl_Files[ASL_FILE_AML_OUTPUT].Filename, Gbl_TableLength);
        }
    }
    else
    {
        FlPrintFile (FileId,
            "%-14s %s - %u lines, %u bytes, %u keywords\n",
            "ASL Input:",
            Gbl_Files[ASL_FILE_INPUT].Filename, Gbl_CurrentLineNumber,
            Gbl_InputByteCount, TotalKeywords);

        /* AML summary */

        if ((Gbl_ExceptionCount[ASL_ERROR] == 0) || (Gbl_IgnoreErrors))
        {
            FlPrintFile (FileId,
                "%-14s %s - %u bytes, %u named objects, %u executable opcodes\n",
                "AML Output:",
                Gbl_Files[ASL_FILE_AML_OUTPUT].Filename, Gbl_TableLength,
                TotalNamedObjects, TotalExecutableOpcodes);
        }
    }

    /* Display summary of any optional files */

    for (i = ASL_FILE_SOURCE_OUTPUT; i <= ASL_MAX_FILE_TYPE; i++)
    {
        if (!Gbl_Files[i].Filename || !Gbl_Files[i].Handle)
        {
            continue;
        }

        /* .SRC is a temp file unless specifically requested */

        if ((i == ASL_FILE_SOURCE_OUTPUT) && (!Gbl_SourceOutputFlag))
        {
            continue;
        }

        /* .I is a temp file unless specifically requested */

        if ((i == ASL_FILE_PREPROCESSOR) && (!Gbl_PreprocessorOutputFlag))
        {
            continue;
        }

        FlPrintFile (FileId, "%14s %s - %u bytes\n",
            Gbl_Files[i].ShortDescription,
            Gbl_Files[i].Filename, FlGetFileSize (i));
    }

    /* Error summary */

    FlPrintFile (FileId,
        "\nCompilation complete. %u Errors, %u Warnings, %u Remarks",
        Gbl_ExceptionCount[ASL_ERROR],
        Gbl_ExceptionCount[ASL_WARNING] +
            Gbl_ExceptionCount[ASL_WARNING2] +
            Gbl_ExceptionCount[ASL_WARNING3],
        Gbl_ExceptionCount[ASL_REMARK]);

    if (Gbl_FileType != ASL_INPUT_TYPE_ASCII_DATA)
    {
        FlPrintFile (FileId,
            ", %u Optimizations", Gbl_ExceptionCount[ASL_OPTIMIZATION]);
    }

    FlPrintFile (FileId, "\n");
}


/*******************************************************************************
 *
 * FUNCTION:    UtCheckIntegerRange
 *
 * PARAMETERS:  Op                  - Integer parse node
 *              LowValue            - Smallest allowed value
 *              HighValue           - Largest allowed value
 *
 * RETURN:      Op if OK, otherwise NULL
 *
 * DESCRIPTION: Check integer for an allowable range
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
UtCheckIntegerRange (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  LowValue,
    UINT32                  HighValue)
{

    if (!Op)
    {
        return (NULL);
    }

    if ((Op->Asl.Value.Integer < LowValue) ||
        (Op->Asl.Value.Integer > HighValue))
    {
        sprintf (MsgBuffer, "0x%X, allowable: 0x%X-0x%X",
            (UINT32) Op->Asl.Value.Integer, LowValue, HighValue);

        AslError (ASL_ERROR, ASL_MSG_RANGE, Op, MsgBuffer);
        return (NULL);
    }

    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    UtGetStringBuffer
 *
 * PARAMETERS:  Length              - Size of buffer requested
 *
 * RETURN:      Pointer to the buffer. Aborts on allocation failure
 *
 * DESCRIPTION: Allocate a string buffer. Bypass the local
 *              dynamic memory manager for performance reasons (This has a
 *              major impact on the speed of the compiler.)
 *
 ******************************************************************************/

char *
UtGetStringBuffer (
    UINT32                  Length)
{
    char                    *Buffer;


    if ((Gbl_StringCacheNext + Length) >= Gbl_StringCacheLast)
    {
        Gbl_StringCacheNext = UtLocalCalloc (ASL_STRING_CACHE_SIZE + Length);
        Gbl_StringCacheLast = Gbl_StringCacheNext + ASL_STRING_CACHE_SIZE +
                                Length;
    }

    Buffer = Gbl_StringCacheNext;
    Gbl_StringCacheNext += Length;

    return (Buffer);
}


/******************************************************************************
 *
 * FUNCTION:    UtExpandLineBuffers
 *
 * PARAMETERS:  None. Updates global line buffer pointers.
 *
 * RETURN:      None. Reallocates the global line buffers
 *
 * DESCRIPTION: Called if the current line buffer becomes filled. Reallocates
 *              all global line buffers and updates Gbl_LineBufferSize. NOTE:
 *              Also used for the initial allocation of the buffers, when
 *              all of the buffer pointers are NULL. Initial allocations are
 *              of size ASL_DEFAULT_LINE_BUFFER_SIZE
 *
 *****************************************************************************/

void
UtExpandLineBuffers (
    void)
{
    UINT32                  NewSize;


    /* Attempt to double the size of all line buffers */

    NewSize = Gbl_LineBufferSize * 2;
    if (Gbl_CurrentLineBuffer)
    {
        DbgPrint (ASL_DEBUG_OUTPUT,"Increasing line buffer size from %u to %u\n",
            Gbl_LineBufferSize, NewSize);
    }

    Gbl_CurrentLineBuffer = realloc (Gbl_CurrentLineBuffer, NewSize);
    Gbl_LineBufPtr = Gbl_CurrentLineBuffer;
    if (!Gbl_CurrentLineBuffer)
    {
        goto ErrorExit;
    }

    Gbl_MainTokenBuffer = realloc (Gbl_MainTokenBuffer, NewSize);
    if (!Gbl_MainTokenBuffer)
    {
        goto ErrorExit;
    }

    Gbl_MacroTokenBuffer = realloc (Gbl_MacroTokenBuffer, NewSize);
    if (!Gbl_MacroTokenBuffer)
    {
        goto ErrorExit;
    }

    Gbl_ExpressionTokenBuffer = realloc (Gbl_ExpressionTokenBuffer, NewSize);
    if (!Gbl_ExpressionTokenBuffer)
    {
        goto ErrorExit;
    }

    Gbl_LineBufferSize = NewSize;
    return;


    /* On error above, simply issue error messages and abort, cannot continue */

ErrorExit:
    printf ("Could not increase line buffer size from %u to %u\n",
        Gbl_LineBufferSize, Gbl_LineBufferSize * 2);

    AslError (ASL_ERROR, ASL_MSG_BUFFER_ALLOCATION,
        NULL, NULL);
    AslAbort ();
}


/*******************************************************************************
 *
 * FUNCTION:    UtInternalizeName
 *
 * PARAMETERS:  ExternalName        - Name to convert
 *              ConvertedName       - Where the converted name is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an external (ASL) name to an internal (AML) name
 *
 ******************************************************************************/

ACPI_STATUS
UtInternalizeName (
    char                    *ExternalName,
    char                    **ConvertedName)
{
    ACPI_NAMESTRING_INFO    Info;
    ACPI_STATUS             Status;


    if (!ExternalName)
    {
        return (AE_OK);
    }

    /* Get the length of the new internal name */

    Info.ExternalName = ExternalName;
    AcpiNsGetInternalNameLength (&Info);

    /* We need a segment to store the internal  name */

    Info.InternalName = UtGetStringBuffer (Info.Length);
    if (!Info.InternalName)
    {
        return (AE_NO_MEMORY);
    }

    /* Build the name */

    Status = AcpiNsBuildInternalName (&Info);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    *ConvertedName = Info.InternalName;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    UtPadNameWithUnderscores
 *
 * PARAMETERS:  NameSeg             - Input nameseg
 *              PaddedNameSeg       - Output padded nameseg
 *
 * RETURN:      Padded nameseg.
 *
 * DESCRIPTION: Pads a NameSeg with underscores if necessary to form a full
 *              ACPI_NAME.
 *
 ******************************************************************************/

static void
UtPadNameWithUnderscores (
    char                    *NameSeg,
    char                    *PaddedNameSeg)
{
    UINT32                  i;


    for (i = 0; (i < ACPI_NAME_SIZE); i++)
    {
        if (*NameSeg)
        {
            *PaddedNameSeg = *NameSeg;
            NameSeg++;
        }
        else
        {
            *PaddedNameSeg = '_';
        }
        PaddedNameSeg++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtAttachNameseg
 *
 * PARAMETERS:  Op                  - Parent parse node
 *              Name                - Full ExternalName
 *
 * RETURN:      None; Sets the NameSeg field in parent node
 *
 * DESCRIPTION: Extract the last nameseg of the ExternalName and store it
 *              in the NameSeg field of the Op.
 *
 ******************************************************************************/

static void
UtAttachNameseg (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name)
{
    char                    *NameSeg;
    char                    PaddedNameSeg[4];


    if (!Name)
    {
        return;
    }

    /* Look for the last dot in the namepath */

    NameSeg = strrchr (Name, '.');
    if (NameSeg)
    {
        /* Found last dot, we have also found the final nameseg */

        NameSeg++;
        UtPadNameWithUnderscores (NameSeg, PaddedNameSeg);
    }
    else
    {
        /* No dots in the namepath, there is only a single nameseg. */
        /* Handle prefixes */

        while (ACPI_IS_ROOT_PREFIX (*Name) ||
               ACPI_IS_PARENT_PREFIX (*Name))
        {
            Name++;
        }

        /* Remaining string should be one single nameseg */

        UtPadNameWithUnderscores (Name, PaddedNameSeg);
    }

    ACPI_MOVE_NAME (Op->Asl.NameSeg, PaddedNameSeg);
}


/*******************************************************************************
 *
 * FUNCTION:    UtAttachNamepathToOwner
 *
 * PARAMETERS:  Op                  - Parent parse node
 *              NameOp              - Node that contains the name
 *
 * RETURN:      Sets the ExternalName and Namepath in the parent node
 *
 * DESCRIPTION: Store the name in two forms in the parent node: The original
 *              (external) name, and the internalized name that is used within
 *              the ACPI namespace manager.
 *
 ******************************************************************************/

void
UtAttachNamepathToOwner (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *NameOp)
{
    ACPI_STATUS             Status;


    /* Full external path */

    Op->Asl.ExternalName = NameOp->Asl.Value.String;

    /* Save the NameOp for possible error reporting later */

    Op->Asl.ParentMethod = (void *) NameOp;

    /* Last nameseg of the path */

    UtAttachNameseg (Op, Op->Asl.ExternalName);

    /* Create internalized path */

    Status = UtInternalizeName (NameOp->Asl.Value.String, &Op->Asl.Namepath);
    if (ACPI_FAILURE (Status))
    {
        /* TBD: abort on no memory */
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtDoConstant
 *
 * PARAMETERS:  String              - Hex, Octal, or Decimal string
 *
 * RETURN:      Converted Integer
 *
 * DESCRIPTION: Convert a string to an integer, with error checking.
 *
 ******************************************************************************/

UINT64
UtDoConstant (
    char                    *String)
{
    ACPI_STATUS             Status;
    UINT64                  Converted;
    char                    ErrBuf[64];


    Status = UtStrtoul64 (String, 0, &Converted);
    if (ACPI_FAILURE (Status))
    {
        sprintf (ErrBuf, "%s %s\n", "Conversion error:",
            AcpiFormatException (Status));
        AslCompilererror (ErrBuf);
    }

    return (Converted);
}


/* TBD: use version in ACPI CA main code base? */

/*******************************************************************************
 *
 * FUNCTION:    UtStrtoul64
 *
 * PARAMETERS:  String              - Null terminated string
 *              Terminater          - Where a pointer to the terminating byte
 *                                    is returned
 *              Base                - Radix of the string
 *
 * RETURN:      Converted value
 *
 * DESCRIPTION: Convert a string into an unsigned value.
 *
 ******************************************************************************/

ACPI_STATUS
UtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    UINT64                  *RetInteger)
{
    UINT32                  Index;
    UINT32                  Sign;
    UINT64                  ReturnValue = 0;
    ACPI_STATUS             Status = AE_OK;


    *RetInteger = 0;

    switch (Base)
    {
    case 0:
    case 8:
    case 10:
    case 16:

        break;

    default:
        /*
         * The specified Base parameter is not in the domain of
         * this function:
         */
        return (AE_BAD_PARAMETER);
    }

    /* Skip over any white space in the buffer: */

    while (isspace ((int) *String) || *String == '\t')
    {
        ++String;
    }

    /*
     * The buffer may contain an optional plus or minus sign.
     * If it does, then skip over it but remember what is was:
     */
    if (*String == '-')
    {
        Sign = NEGATIVE;
        ++String;
    }
    else if (*String == '+')
    {
        ++String;
        Sign = POSITIVE;
    }
    else
    {
        Sign = POSITIVE;
    }

    /*
     * If the input parameter Base is zero, then we need to
     * determine if it is octal, decimal, or hexadecimal:
     */
    if (Base == 0)
    {
        if (*String == '0')
        {
            if (tolower ((int) *(++String)) == 'x')
            {
                Base = 16;
                ++String;
            }
            else
            {
                Base = 8;
            }
        }
        else
        {
            Base = 10;
        }
    }

    /*
     * For octal and hexadecimal bases, skip over the leading
     * 0 or 0x, if they are present.
     */
    if (Base == 8 && *String == '0')
    {
        String++;
    }

    if (Base == 16 &&
        *String == '0' &&
        tolower ((int) *(++String)) == 'x')
    {
        String++;
    }

    /* Main loop: convert the string to an unsigned long */

    while (*String)
    {
        if (isdigit ((int) *String))
        {
            Index = ((UINT8) *String) - '0';
        }
        else
        {
            Index = (UINT8) toupper ((int) *String);
            if (isupper ((int) Index))
            {
                Index = Index - 'A' + 10;
            }
            else
            {
                goto ErrorExit;
            }
        }

        if (Index >= Base)
        {
            goto ErrorExit;
        }

        /* Check to see if value is out of range: */

        if (ReturnValue > ((ACPI_UINT64_MAX - (UINT64) Index) /
                            (UINT64) Base))
        {
            goto ErrorExit;
        }
        else
        {
            ReturnValue *= Base;
            ReturnValue += Index;
        }

        ++String;
    }


    /* If a minus sign was present, then "the conversion is negated": */

    if (Sign == NEGATIVE)
    {
        ReturnValue = (ACPI_UINT32_MAX - ReturnValue) + 1;
    }

    *RetInteger = ReturnValue;
    return (Status);


ErrorExit:
    switch (Base)
    {
    case 8:

        Status = AE_BAD_OCTAL_CONSTANT;
        break;

    case 10:

        Status = AE_BAD_DECIMAL_CONSTANT;
        break;

    case 16:

        Status = AE_BAD_HEX_CONSTANT;
        break;

    default:

        /* Base validated above */

        break;
    }

    return (Status);
}
