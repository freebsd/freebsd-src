
/******************************************************************************
 *
 * Module Name: aslutils -- compiler utilities
 *              $Revision: 1.66 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/amlcode.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslutils")

#ifdef _USE_BERKELEY_YACC
extern const char * const       AslCompilername[];
static const char * const       *yytname = &AslCompilername[254];
#else
extern const char * const       yytname[];
#endif

/* Local prototypes */

static ACPI_STATUS
UtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    ACPI_INTEGER            *RetInteger);

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
 * FUNCTION:    AcpiPsDisplayConstantOpcodes
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
    UINT32              i;


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
 * PARAMETERS:  Size        - Bytes to be allocated
 *
 * RETURN:      Pointer to the allocated memory.  Guaranteed to be valid.
 *
 * DESCRIPTION: Allocate zero-initialized memory.  Aborts the compile on an
 *              allocation failure, on the assumption that nothing more can be
 *              accomplished.
 *
 ******************************************************************************/

void *
UtLocalCalloc (
    UINT32                  Size)
{
    void                    *Allocated;


    Allocated = ACPI_MEM_CALLOCATE (Size);
    if (!Allocated)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_MEMORY_ALLOCATION,
            Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
            Gbl_InputByteCount, Gbl_CurrentColumn,
            Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
        exit (1);
    }

    TotalAllocations++;
    TotalAllocated += Size;
    return Allocated;
}


/*******************************************************************************
 *
 * FUNCTION:    UtBeginEvent
 *
 * PARAMETERS:  Name        - Ascii name of this event
 *
 * RETURN:      Event       - Event number (integer index)
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
 * PARAMETERS:  Event       - Event number (integer index)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Saves the current time (end time) with this event
 *
 ******************************************************************************/

void
UtEndEvent (
    UINT8                  Event)
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
 * PARAMETERS:  HexChar         - Hex character in Ascii
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
 * PARAMETERS:  RawByte         - Binary data
 *              Buffer          - Pointer to where the hex bytes will be stored
 *
 * RETURN:      Ascii hex byte is stored in Buffer.
 *
 * DESCRIPTION: Perform hex-to-ascii translation.  The return data is prefixed
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

    Buffer[2] = (UINT8) hex[(RawByte >> 4) & 0xF];
    Buffer[3] = (UINT8) hex[RawByte & 0xF];
}


/*******************************************************************************
 *
 * FUNCTION:    UtConvertByteToAsmHex
 *
 * PARAMETERS:  RawByte         - Binary data
 *              Buffer          - Pointer to where the hex bytes will be stored
 *
 * RETURN:      Ascii hex byte is stored in Buffer.
 *
 * DESCRIPTION: Perform hex-to-ascii translation.  The return data is prefixed
 *              with "0x"
 *
 ******************************************************************************/

void
UtConvertByteToAsmHex (
    UINT8                   RawByte,
    UINT8                   *Buffer)
{

    Buffer[0] = '0';

    Buffer[1] = (UINT8) hex[(RawByte >> 4) & 0xF];
    Buffer[2] = (UINT8) hex[RawByte & 0xF];
    Buffer[3] = 'h';
}


/*******************************************************************************
 *
 * FUNCTION:    DbgPrint
 *
 * PARAMETERS:  Type            - Type of output
 *              Fmt             - Printf format string
 *              ...             - variable printf list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Conditional print statement.  Prints to stderr only if the
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


    va_start (Args, Fmt);

    if (!Gbl_DebugFlag)
    {
        return;
    }

    if ((Type == ASL_PARSE_OUTPUT) &&
        (!(AslCompilerdebug)))
    {
        return;
    }

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

    DbgPrint (ASL_TREE_OUTPUT,
        "%*s %-16.16s", (3 * Level), " ",
        UtGetOpName (ParseOpcode));

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
 * PARAMETERS:  Op
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

    strncpy (Op->Asl.ParseOpName, UtGetOpName (Op->Asl.ParseOpcode), 12);
}


/*******************************************************************************
 *
 * FUNCTION:    UtGetOpName
 *
 * PARAMETERS:  ParseOpcode         - Parser keyword ID
 *
 * RETURN:      Pointer to the opcode name
 *
 * DESCRIPTION: Get the ascii name of the parse opcode
 *
 ******************************************************************************/

char *
UtGetOpName (
    UINT32                  ParseOpcode)
{

    /*
     * First entries (ASL_YYTNAME_START) in yytname are special reserved names.
     * Ignore first 8 characters of the name
     */
    return ((char *) yytname [(ParseOpcode - ASL_FIRST_PARSE_OPCODE) + ASL_YYTNAME_START] + 8);
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplaySummary
 *
 * PARAMETERS:  FileID          - ID of outpout file
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

    if (FileId != ASL_FILE_STDOUT)
    {
        /* Compiler name and version number */

        FlPrintFile (FileId, "%s version %X [%s]\n",
            CompilerId, (UINT32) ACPI_CA_VERSION, __DATE__);
    }

    /* Input/Output summary */

    FlPrintFile (FileId,
        "ASL Input:  %s - %d lines, %d bytes, %d keywords\n",
        Gbl_Files[ASL_FILE_INPUT].Filename, Gbl_CurrentLineNumber,
        Gbl_InputByteCount, TotalKeywords);

    /* AML summary */

    if ((Gbl_ExceptionCount[ASL_ERROR] == 0) || (Gbl_IgnoreErrors))
    {
        FlPrintFile (FileId,
            "AML Output: %s - %d bytes %d named objects %d executable opcodes\n\n",
            Gbl_Files[ASL_FILE_AML_OUTPUT].Filename, Gbl_TableLength,
            TotalNamedObjects, TotalExecutableOpcodes);
    }

    /* Error summary */

    FlPrintFile (FileId,
        "Compilation complete. %d Errors, %d Warnings, %d Remarks, %d Optimizations\n",
        Gbl_ExceptionCount[ASL_ERROR],
        Gbl_ExceptionCount[ASL_WARNING],
        Gbl_ExceptionCount[ASL_REMARK],
        Gbl_ExceptionCount[ASL_OPTIMIZATION]);
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplaySummary
 *
 * PARAMETERS:  Op            - Integer parse node
 *              LowValue        - Smallest allowed value
 *              HighValue       - Largest allowed value
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
    char                    *ParseError = NULL;
    char                    Buffer[64];


    if (!Op)
    {
        return NULL;
    }

    if (Op->Asl.Value.Integer < LowValue)
    {
        ParseError = "Value below valid range";
        Op->Asl.Value.Integer = LowValue;
    }

    if (Op->Asl.Value.Integer > HighValue)
    {
        ParseError = "Value above valid range";
        Op->Asl.Value.Integer = HighValue;
    }

    if (ParseError)
    {
        sprintf (Buffer, "%s 0x%X-0x%X", ParseError, LowValue, HighValue);
        AslCompilererror (Buffer);

        return NULL;
    }

    return Op;
}


/*******************************************************************************
 *
 * FUNCTION:    UtGetStringBuffer
 *
 * PARAMETERS:  Length          - Size of buffer requested
 *
 * RETURN:      Pointer to the buffer.  Aborts on allocation failure
 *
 * DESCRIPTION: Allocate a string buffer.  Bypass the local
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


/*******************************************************************************
 *
 * FUNCTION:    UtInternalizeName
 *
 * PARAMETERS:  ExternalName            - Name to convert
 *              ConvertedName           - Where the converted name is returned
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
 * PARAMETERS:  NameSeg         - Input nameseg
 *              PaddedNameSeg   - Output padded nameseg
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
 * PARAMETERS:  Op              - Parent parse node
 *              Name            - Full ExternalName
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

        while ((*Name == '\\') || (*Name == '^'))
        {
            Name++;
        }

        /* Remaing string should be one single nameseg */

        UtPadNameWithUnderscores (Name, PaddedNameSeg);
    }

    strncpy (Op->Asl.NameSeg, PaddedNameSeg, 4);
}


/*******************************************************************************
 *
 * FUNCTION:    UtAttachNamepathToOwner
 *
 * PARAMETERS:  Op            - Parent parse node
 *              NameOp        - Node that contains the name
 *
 * RETURN:      Sets the ExternalName and Namepath in the parent node
 *
 * DESCRIPTION: Store the name in two forms in the parent node:  The original
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
 * PARAMETERS:  String      - Hex, Octal, or Decimal string
 *
 * RETURN:      Converted Integer
 *
 * DESCRIPTION: Convert a string to an integer.  With error checking.
 *
 ******************************************************************************/

ACPI_INTEGER
UtDoConstant (
    char                    *String)
{
    ACPI_STATUS             Status;
    ACPI_INTEGER            Converted;
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
 * PARAMETERS:  String          - Null terminated string
 *              Terminater      - Where a pointer to the terminating byte is
 *                                returned
 *              Base            - Radix of the string
 *
 * RETURN:      Converted value
 *
 * DESCRIPTION: Convert a string into an unsigned value.
 *
 ******************************************************************************/

static ACPI_STATUS
UtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    ACPI_INTEGER            *RetInteger)
{
    UINT32                  Index;
    UINT32                  Sign;
    ACPI_INTEGER            ReturnValue = 0;
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

    while (isspace (*String) || *String == '\t')
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
            if (tolower (*(++String)) == 'x')
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
        tolower (*(++String)) == 'x')
    {
        String++;
    }

    /* Main loop: convert the string to an unsigned long */

    while (*String)
    {
        if (isdigit (*String))
        {
            Index = ((UINT8) *String) - '0';
        }
        else
        {
            Index = (UINT8) toupper (*String);
            if (isupper ((char) Index))
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

        if (ReturnValue > ((ACPI_INTEGER_MAX - (ACPI_INTEGER) Index) /
                            (ACPI_INTEGER) Base))
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


