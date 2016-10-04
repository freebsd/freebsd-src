/******************************************************************************
 *
 * Module Name: aslutils -- compiler utilities
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
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acapps.h>
#include <sys/stat.h>


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
 * FUNCTION:    UtIsBigEndianMachine
 *
 * PARAMETERS:  None
 *
 * RETURN:      TRUE if machine is big endian
 *              FALSE if machine is little endian
 *
 * DESCRIPTION: Detect whether machine is little endian or big endian.
 *
 ******************************************************************************/

UINT8
UtIsBigEndianMachine (
    void)
{
    union {
        UINT32              Integer;
        UINT8               Bytes[4];
    } Overlay =                 {0xFF000000};


    return (Overlay.Bytes[0]); /* Returns 0xFF (TRUE) for big endian */
}


/******************************************************************************
 *
 * FUNCTION:    UtQueryForOverwrite
 *
 * PARAMETERS:  Pathname            - Output filename
 *
 * RETURN:      TRUE if file does not exist or overwrite is authorized
 *
 * DESCRIPTION: Query for file overwrite if it already exists.
 *
 ******************************************************************************/

BOOLEAN
UtQueryForOverwrite (
    char                    *Pathname)
{
    struct stat             StatInfo;


    if (!stat (Pathname, &StatInfo))
    {
        fprintf (stderr, "Target file \"%s\" already exists, overwrite? [y|n] ",
            Pathname);

        if (getchar () != 'y')
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


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

void
UtDisplaySupportedTables (
    void)
{
    const AH_TABLE          *TableData;
    UINT32                  i;


    printf ("\nACPI tables supported by iASL version %8.8X:\n"
        "  (Compiler, Disassembler, Template Generator)\n\n",
        ACPI_CA_VERSION);

    /* All ACPI tables with the common table header */

    printf ("\n  Supported ACPI tables:\n");
    for (TableData = AcpiSupportedTables, i = 1;
         TableData->Signature; TableData++, i++)
    {
        printf ("%8u) %s    %s\n", i,
            TableData->Signature, TableData->Description);
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
            Gbl_OriginalInputFileSize, TotalKeywords);

        /* AML summary */

        if ((Gbl_ExceptionCount[ASL_ERROR] == 0) || (Gbl_IgnoreErrors))
        {
            if (Gbl_Files[ASL_FILE_AML_OUTPUT].Handle)
            {
                FlPrintFile (FileId,
                    "%-14s %s - %u bytes, %u named objects, "
                    "%u executable opcodes\n",
                    "AML Output:",
                    Gbl_Files[ASL_FILE_AML_OUTPUT].Filename,
                    FlGetFileSize (ASL_FILE_AML_OUTPUT),
                    TotalNamedObjects, TotalExecutableOpcodes);
            }
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

        /* .PRE is the preprocessor intermediate file */

        if ((i == ASL_FILE_PREPROCESSOR)  && (!Gbl_KeepPreprocessorTempFile))
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
        FlPrintFile (FileId, ", %u Optimizations",
            Gbl_ExceptionCount[ASL_OPTIMIZATION]);

        if (TotalFolds)
        {
            FlPrintFile (FileId, ", %u Constants Folded", TotalFolds);
        }
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
 * FUNCTION:    UtStringCacheCalloc
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
UtStringCacheCalloc (
    UINT32                  Length)
{
    char                    *Buffer;
    ASL_CACHE_INFO          *Cache;
    UINT32                  CacheSize = ASL_STRING_CACHE_SIZE;


    if (Length > CacheSize)
    {
        CacheSize = Length;

        if (Gbl_StringCacheList)
        {
            Cache = UtLocalCalloc (sizeof (Cache->Next) + CacheSize);

            /* Link new cache buffer just following head of list */

            Cache->Next = Gbl_StringCacheList->Next;
            Gbl_StringCacheList->Next = Cache;

            /* Leave cache management pointers alone as they pertain to head */

            Gbl_StringCount++;
            Gbl_StringSize += Length;

            return (Cache->Buffer);
        }
    }

    if ((Gbl_StringCacheNext + Length) >= Gbl_StringCacheLast)
    {
        /* Allocate a new buffer */

        Cache = UtLocalCalloc (sizeof (Cache->Next) + CacheSize);

        /* Link new cache buffer to head of list */

        Cache->Next = Gbl_StringCacheList;
        Gbl_StringCacheList = Cache;

        /* Setup cache management pointers */

        Gbl_StringCacheNext = Cache->Buffer;
        Gbl_StringCacheLast = Gbl_StringCacheNext + CacheSize;
    }

    Gbl_StringCount++;
    Gbl_StringSize += Length;

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
        DbgPrint (ASL_DEBUG_OUTPUT,
            "Increasing line buffer size from %u to %u\n",
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


/******************************************************************************
 *
 * FUNCTION:    UtFreeLineBuffers
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Free all line buffers
 *
 *****************************************************************************/

void
UtFreeLineBuffers (
    void)
{

    free (Gbl_CurrentLineBuffer);
    free (Gbl_MainTokenBuffer);
    free (Gbl_MacroTokenBuffer);
    free (Gbl_ExpressionTokenBuffer);
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

    /* We need a segment to store the internal name */

    Info.InternalName = UtStringCacheCalloc (Info.Length);
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
 * PARAMETERS:  String              - Hexadecimal or decimal string
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


    Status = AcpiUtStrtoul64 (String, ACPI_STRTOUL_64BIT, &Converted);
    if (ACPI_FAILURE (Status))
    {
        sprintf (ErrBuf, "%s %s\n", "Conversion error:",
            AcpiFormatException (Status));
        AslCompilererror (ErrBuf);
    }

    return (Converted);
}
