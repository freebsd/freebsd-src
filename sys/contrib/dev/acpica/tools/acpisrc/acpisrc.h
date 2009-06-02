
/******************************************************************************
 *
 * Module Name: acpisrc.h - Include file for AcpiSrc utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
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


#define LINES_IN_LEGAL_HEADER               105 /* See above */
#define LEGAL_HEADER_SIGNATURE              " * 2.1. This is your license from Intel Corp. under its intellectual property"
#define LINES_IN_LINUX_HEADER               34
#define LINUX_HEADER_SIGNATURE              " * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS"
#define LINES_IN_ASL_HEADER                 29 /* Header as output from disassembler */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#ifdef WIN32
#include <io.h>
#include <direct.h>
#endif
#include <errno.h>

#include "acpi.h"
#include "accommon.h"


/* Fixups for non-Win32 compilation */
#ifndef WIN32
#ifndef O_BINARY
#define O_BINARY    0x0
#endif

#define mkdir(x) mkdir(x, 0770)
char * strlwr(char* str);
#endif


/* Constants */

#define ASRC_MAX_FILE_SIZE                  (1024 * 100)

#define FILE_TYPE_SOURCE                    1
#define FILE_TYPE_HEADER                    2
#define FILE_TYPE_DIRECTORY                 3

#define CVT_COUNT_TABS                      0x00000001
#define CVT_COUNT_NON_ANSI_COMMENTS         0x00000002
#define CVT_TRIM_LINES                      0x00000004
#define CVT_CHECK_BRACES                    0x00000008
#define CVT_COUNT_LINES                     0x00000010
#define CVT_BRACES_ON_SAME_LINE             0x00000020
#define CVT_MIXED_CASE_TO_UNDERSCORES       0x00000040
#define CVT_LOWER_CASE_IDENTIFIERS          0x00000080
#define CVT_REMOVE_DEBUG_MACROS             0x00000100
#define CVT_TRIM_WHITESPACE                 0x00000200  /* Should be after all line removal */
#define CVT_REMOVE_EMPTY_BLOCKS             0x00000400  /* Should be after trimming lines */
#define CVT_REDUCE_TYPEDEFS                 0x00000800
#define CVT_COUNT_SHORTMULTILINE_COMMENTS   0x00001000
#define CVT_SPACES_TO_TABS4                 0x40000000  /* Tab conversion should be last */
#define CVT_SPACES_TO_TABS8                 0x80000000  /* Tab conversion should be last */

#define FLG_DEFAULT_FLAGS                   0x00000000
#define FLG_NO_CARRIAGE_RETURNS             0x00000001
#define FLG_NO_FILE_OUTPUT                  0x00000002
#define FLG_LOWERCASE_DIRNAMES              0x00000004

#define AS_START_IGNORE                     "/*!"
#define AS_STOP_IGNORE                      "!*/"


/* Globals */

extern UINT32                   Gbl_Files;
extern UINT32                   Gbl_MissingBraces;
extern UINT32                   Gbl_Tabs;
extern UINT32                   Gbl_NonAnsiComments;
extern UINT32                   Gbl_SourceLines;
extern UINT32                   Gbl_WhiteLines;
extern UINT32                   Gbl_CommentLines;
extern UINT32                   Gbl_LongLines;
extern UINT32                   Gbl_TotalLines;
extern UINT32                   Gbl_HeaderSize;
extern UINT32                   Gbl_HeaderLines;
extern struct stat              Gbl_StatBuf;
extern char                     *Gbl_FileBuffer;
extern UINT32                   Gbl_TotalSize;
extern UINT32                   Gbl_FileSize;
extern UINT32                   Gbl_FileType;
extern BOOLEAN                  Gbl_VerboseMode;
extern BOOLEAN                  Gbl_QuietMode;
extern BOOLEAN                  Gbl_BatchMode;
extern BOOLEAN                  Gbl_MadeChanges;
extern BOOLEAN                  Gbl_Overwrite;
extern BOOLEAN                  Gbl_WidenDeclarations;
extern BOOLEAN                  Gbl_IgnoreLoneLineFeeds;
extern BOOLEAN                  Gbl_HasLoneLineFeeds;
extern void                     *Gbl_StructDefs;

#define PARAM_LIST(pl)          pl
#define TERSE_PRINT(a)          if (!Gbl_VerboseMode) printf PARAM_LIST(a)
#define VERBOSE_PRINT(a)        if (Gbl_VerboseMode) printf PARAM_LIST(a)

#define REPLACE_WHOLE_WORD      0x00
#define REPLACE_SUBSTRINGS      0x01
#define REPLACE_MASK            0x01

#define EXTRA_INDENT_C          0x02


/* Conversion table structs */

typedef struct acpi_string_table
{
    char                        *Target;
    char                        *Replacement;
    UINT8                       Type;

} ACPI_STRING_TABLE;


typedef struct acpi_typed_identifier_table
{
    char                        *Identifier;
    UINT8                       Type;

} ACPI_TYPED_IDENTIFIER_TABLE;

#define SRC_TYPE_SIMPLE         0
#define SRC_TYPE_STRUCT         1
#define SRC_TYPE_UNION          2


typedef struct acpi_identifier_table
{
    char                        *Identifier;

} ACPI_IDENTIFIER_TABLE;

typedef struct acpi_conversion_table
{
    char                        *NewHeader;
    UINT32                      Flags;

    ACPI_TYPED_IDENTIFIER_TABLE *LowerCaseTable;

    ACPI_STRING_TABLE           *SourceStringTable;
    ACPI_IDENTIFIER_TABLE       *SourceLineTable;
    ACPI_IDENTIFIER_TABLE       *SourceConditionalTable;
    ACPI_IDENTIFIER_TABLE       *SourceMacroTable;
    ACPI_TYPED_IDENTIFIER_TABLE *SourceStructTable;
    UINT32                      SourceFunctions;

    ACPI_STRING_TABLE           *HeaderStringTable;
    ACPI_IDENTIFIER_TABLE       *HeaderLineTable;
    ACPI_IDENTIFIER_TABLE       *HeaderConditionalTable;
    ACPI_IDENTIFIER_TABLE       *HeaderMacroTable;
    ACPI_TYPED_IDENTIFIER_TABLE *HeaderStructTable;
    UINT32                      HeaderFunctions;

} ACPI_CONVERSION_TABLE;


/* Conversion tables */

extern ACPI_CONVERSION_TABLE       LinuxConversionTable;
extern ACPI_CONVERSION_TABLE       CleanupConversionTable;
extern ACPI_CONVERSION_TABLE       StatsConversionTable;
extern ACPI_CONVERSION_TABLE       CustomConversionTable;


/* Prototypes */

char *
AsSkipUntilChar (
    char                    *Buffer,
    char                    Target);

char *
AsSkipPastChar (
    char                    *Buffer,
    char                    Target);

char *
AsReplaceData (
    char                    *Buffer,
    UINT32                  LengthToRemove,
    char                    *BufferToAdd,
    UINT32                  LengthToAdd);

int
AsReplaceString (
    char                    *Target,
    char                    *Replacement,
    UINT8                   Type,
    char                    *Buffer);

int
AsLowerCaseString (
    char                    *Target,
    char                    *Buffer);

void
AsRemoveLine (
    char                    *Buffer,
    char                    *Keyword);

void
AsRemoveMacro (
    char                    *Buffer,
    char                    *Keyword);

void
AsCheckForBraces (
    char                    *Buffer,
    char                    *Filename);

void
AsTrimLines (
    char                    *Buffer,
    char                    *Filename);

void
AsMixedCaseToUnderscores (
    char                    *Buffer);

void
AsCountTabs (
    char                    *Buffer,
    char                    *Filename);

void
AsBracesOnSameLine (
    char                    *Buffer);

void
AsLowerCaseIdentifiers (
    char                    *Buffer);

void
AsReduceTypedefs (
    char                    *Buffer,
    char                    *Keyword);

void
AsRemoveDebugMacros (
    char                    *Buffer);

void
AsRemoveEmptyBlocks (
    char                    *Buffer,
    char                    *Filename);

void
AsCountSourceLines (
    char                    *Buffer,
    char                    *Filename);

void
AsCountNonAnsiComments (
    char                    *Buffer,
    char                    *Filename);

void
AsTrimWhitespace (
    char                    *Buffer);

void
AsTabify4 (
    char                    *Buffer);

void
AsTabify8 (
    char                    *Buffer);

void
AsRemoveConditionalCompile (
    char                    *Buffer,
    char                    *Keyword);

ACPI_NATIVE_INT
AsProcessTree (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *SourcePath,
    char                    *TargetPath);

int
AsGetFile (
    char                    *FileName,
    char                    **FileBuffer,
    UINT32                  *FileSize);

int
AsPutFile (
    char                    *Pathname,
    char                    *FileBuffer,
    UINT32                  SystemFlags);

void
AsReplaceHeader (
    char                    *Buffer,
    char                    *NewHeader);

void
AsConvertFile (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *FileBuffer,
    char                    *Filename,
    ACPI_NATIVE_INT         FileType);

ACPI_NATIVE_INT
AsProcessOneFile (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *SourcePath,
    char                    *TargetPath,
    int                     MaxPathLength,
    char                    *Filename,
    ACPI_NATIVE_INT         FileType);

ACPI_NATIVE_INT
AsCheckForDirectory (
    char                    *SourceDirPath,
    char                    *TargetDirPath,
    char                    *Filename,
    char                    **SourcePath,
    char                    **TargetPath);

BOOLEAN
AsMatchExactWord (
    char                    *Word,
    UINT32                  WordLength);

void
AsPrint (
    char                    *Message,
    UINT32                  Count,
    char                    *Filename);

void
AsInsertPrefix (
    char                    *Buffer,
    char                    *Keyword,
    UINT8                   Type);

char *
AsInsertData (
    char                    *Buffer,
    char                    *BufferToAdd,
    UINT32                  LengthToAdd);

char *
AsRemoveData (
    char                    *StartPointer,
    char                    *EndPointer);

void
AsInsertCarriageReturns (
    char                    *Buffer);

void
AsConvertToLineFeeds (
    char                    *Buffer);


