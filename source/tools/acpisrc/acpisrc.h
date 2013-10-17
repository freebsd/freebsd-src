/******************************************************************************
 *
 * Module Name: acpisrc.h - Include file for AcpiSrc utility
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

#include "acpi.h"
#include "accommon.h"

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

/* mkdir support */

#ifdef WIN32
#include <direct.h>
#else
#define mkdir(x) mkdir(x, 0770)
#endif


/* Constants */

#define LINES_IN_LEGAL_HEADER               105 /* See above */
#define LEGAL_HEADER_SIGNATURE              " * 2.1. This is your license from Intel Corp. under its intellectual property"
#define LINES_IN_LINUX_HEADER               34
#define LINUX_HEADER_SIGNATURE              " * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS"
#define LINES_IN_ASL_HEADER                 29 /* Header as output from disassembler */

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
extern BOOLEAN                  Gbl_Cleanup;
extern BOOLEAN                  Gbl_IgnoreTranslationEscapes;
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
    ACPI_IDENTIFIER_TABLE       *SourceSpecialMacroTable;
    UINT32                      SourceFunctions;

    ACPI_STRING_TABLE           *HeaderStringTable;
    ACPI_IDENTIFIER_TABLE       *HeaderLineTable;
    ACPI_IDENTIFIER_TABLE       *HeaderConditionalTable;
    ACPI_IDENTIFIER_TABLE       *HeaderMacroTable;
    ACPI_TYPED_IDENTIFIER_TABLE *HeaderStructTable;
    ACPI_IDENTIFIER_TABLE       *HeaderSpecialMacroTable;
    UINT32                      HeaderFunctions;

} ACPI_CONVERSION_TABLE;


/* Conversion tables */

extern ACPI_CONVERSION_TABLE       LinuxConversionTable;
extern ACPI_CONVERSION_TABLE       CleanupConversionTable;
extern ACPI_CONVERSION_TABLE       StatsConversionTable;
extern ACPI_CONVERSION_TABLE       CustomConversionTable;
extern ACPI_CONVERSION_TABLE       LicenseConversionTable;
extern ACPI_CONVERSION_TABLE       IndentConversionTable;


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
    char                    *Buffer,
    char                    *Filename);

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
AsCleanupSpecialMacro (
    char                    *Buffer,
    char                    *Keyword);

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

void
AsRemoveExtraLines (
    char                    *FileBuffer,
    char                    *Filename);

void
AsRemoveSpacesAfterPeriod (
    char                    *FileBuffer,
    char                    *Filename);

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

void
AsStrlwr (
    char                    *SrcString);
