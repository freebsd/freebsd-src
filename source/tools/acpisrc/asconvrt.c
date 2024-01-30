/******************************************************************************
 *
 * Module Name: asconvrt - Source conversion code
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "acpisrc.h"

AS_BRACE_INFO               Gbl_BraceInfo[] =
{
    {" if",         3},
    {" else if",    8},
    {" else while", 11},
    {" else",       5},
    {" do ",        4},
    {NULL,          0}
};


/* Local prototypes */

static char *
AsMatchValidToken (
    char                    *Buffer,
    char                    *Filename,
    char                    TargetChar,
    AS_SCAN_CALLBACK        Callback);

static char *
AsCheckBracesCallback (
    char                    *Buffer,
    char                    *Filename,
    UINT32                  LineNumber);

static UINT32
AsCountLines (
    char                    *Buffer,
    char                    *Filename);


#define MODULE_HEADER_BEGIN "/******************************************************************************\n *\n * Module Name:";
#define MODULE_HEADER_END   " *****************************************************************************/\n\n"
#define INTEL_COPYRIGHT     " * Copyright (C) 2000 - 2023, Intel Corp.\n"

/* Opening signature of the Intel legal header */

char        *HeaderBegin = "/******************************************************************************\n *\n * 1. Copyright Notice";

UINT32      NonAnsiCommentCount;

char        CopyRightHeaderEnd[] = INTEL_COPYRIGHT " *\n" MODULE_HEADER_END;

/******************************************************************************
 *
 * FUNCTION:    AsCountNonAnsiComments
 *
 * DESCRIPTION: Count the number of "//" comments. This type of comment is
 *              non-ANSI C.
 *
 * NOTE: July 2014: Allows // within quoted strings and within normal
 *       comments. Eliminates extraneous warnings from this utility.
 *
 ******************************************************************************/

void
AsCountNonAnsiComments (
    char                    *Buffer,
    char                    *Filename)
{

    AsMatchValidToken (Buffer, Filename, 0, NULL);

    /* Error if any slash-slash comments found */

    if (NonAnsiCommentCount)
    {
        AsPrint ("Non-ANSI // Comments Found", NonAnsiCommentCount, Filename);
        Gbl_NonAnsiComments += NonAnsiCommentCount;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsCheckForBraces
 *
 * DESCRIPTION: Check for an open brace after each if/else/do (etc.)
 *              statement
 *
 ******************************************************************************/

void
AsCheckForBraces (
    char                    *Buffer,
    char                    *Filename)
{

    AsMatchValidToken (Buffer, Filename, 0, AsCheckBracesCallback);
}


/******************************************************************************
 *
 * FUNCTION:    AsCheckBracesCallback
 *
 * DESCRIPTION: Check if/else/do statements. Ensure that braces
 *              are always used.
 *
 * TBD: Currently, don't check while() statements. The problem is that there
 * are two forms: do {} while (); and while () {}.
 *
 ******************************************************************************/

static char *
AsCheckBracesCallback (
    char                    *Buffer,
    char                    *Filename,
    UINT32                  LineNumber)
{
    char                    *SubBuffer = Buffer;
    char                    *NextBrace;
    char                    *NextSemicolon;
    AS_BRACE_INFO           *BraceInfo;


    for (BraceInfo = Gbl_BraceInfo; BraceInfo->Operator; BraceInfo++)
    {
        if (!(strncmp (BraceInfo->Operator, SubBuffer, BraceInfo->Length)))
        {
            SubBuffer += (BraceInfo->Length - 1);

            /* Find next brace and the next semicolon */

            NextBrace = AsMatchValidToken (SubBuffer, Filename, '{', NULL);
            NextSemicolon = AsMatchValidToken (SubBuffer, Filename, ';', NULL);

            /* Next brace should appear before next semicolon */

            if ((!NextBrace) ||
               (NextSemicolon && (NextBrace > NextSemicolon)))
            {
                Gbl_MissingBraces++;

                if (!Gbl_QuietMode)
                {
                    printf ("Missing braces for <%s>, line %u: %s\n",
                        BraceInfo->Operator + 1, LineNumber, Filename);
                }
            }

            return (SubBuffer);
        }
    }

    /* No match, just return original buffer */

    return (Buffer);
}


/******************************************************************************
 *
 * FUNCTION:    AsMatchValidToken
 *
 * DESCRIPTION: Find the next matching token in the input buffer.
 *
 ******************************************************************************/

static char *
AsMatchValidToken (
    char                    *Buffer,
    char                    *Filename,
    char                    TargetChar,
    AS_SCAN_CALLBACK        Callback)
{
    char                    *SubBuffer = Buffer;
    char                    *StringStart;
    UINT32                  TotalLines;


    TotalLines = 1;
    NonAnsiCommentCount = 0;

    /* Scan from current position up to the end if necessary */

    while (*SubBuffer)
    {
        /* Skip normal comments */

        if ((*SubBuffer == '/') &&
            (*(SubBuffer + 1) == '*'))
        {
            /* Must maintain line count */

            SubBuffer += 2;
            while (strncmp ("*/", SubBuffer, 2))
            {
                if (*SubBuffer == '\n')
                {
                    TotalLines++;
                }
                SubBuffer++;
            }

            SubBuffer += 2;
            continue;
        }

        /* Skip single quoted chars */

        if (*SubBuffer == '\'')
        {
            SubBuffer++;
            if (!(*SubBuffer))
            {
                break;
            }

            if (*SubBuffer == '\\')
            {
                SubBuffer++;
            }

            SubBuffer++;
            continue;
        }

        /* Skip quoted strings */

        if (*SubBuffer == '"')
        {
            StringStart = SubBuffer;
            SubBuffer++;
            if (!(*SubBuffer))
            {
                break;
            }

            while (*SubBuffer != '"')
            {
                if ((*SubBuffer == '\n') ||
                    (!(*SubBuffer)))
                {
                    AsPrint ("Unbalanced quoted string",1, Filename);
                    printf ("    %.32s (line %u)\n", StringStart, TotalLines);
                    break;
                }

                /* Handle escapes within the string */

                if (*SubBuffer == '\\')
                {
                    SubBuffer++;
                }

                SubBuffer++;
            }

            SubBuffer++;
            continue;
        }

        /* Now we can check for a slash-slash comment */

        if ((*SubBuffer == '/') &&
            (*(SubBuffer + 1) == '/'))
        {
            NonAnsiCommentCount++;

            /* Skip to end-of-line */

            while ((*SubBuffer != '\n') &&
                (*SubBuffer))
            {
                SubBuffer++;
            }

            if (!(*SubBuffer))
            {
                break;
            }

            if (*SubBuffer == '\n')
            {
                TotalLines++;
            }

            SubBuffer++;
            continue;
        }

        /* Finally, check for a newline */

        if (*SubBuffer == '\n')
        {
            TotalLines++;
            SubBuffer++;
            continue;
        }

        /* Normal character, do the user actions */

        if (Callback)
        {
            SubBuffer = Callback (SubBuffer, Filename, TotalLines);
        }

        if (TargetChar && (*SubBuffer == TargetChar))
        {
            return (SubBuffer);
        }

        SubBuffer++;
    }

    return (NULL);
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveExtraLines
 *
 * DESCRIPTION: Remove all extra lines at the start and end of the file.
 *
 ******************************************************************************/

void
AsRemoveExtraLines (
    char                    *FileBuffer,
    char                    *Filename)
{
    char                    *FileEnd;
    int                     Length;


    /* Remove any extra lines at the start of the file */

    while (*FileBuffer == '\n')
    {
        printf ("Removing extra line at start of file: %s\n", Filename);
        AsRemoveData (FileBuffer, FileBuffer + 1);
    }

    /* Remove any extra lines at the end of the file */

    Length = strlen (FileBuffer);
    FileEnd = FileBuffer + (Length - 2);

    while (*FileEnd == '\n')
    {
        printf ("Removing extra line at end of file: %s\n", Filename);
        AsRemoveData (FileEnd, FileEnd + 1);
        FileEnd--;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveSpacesAfterPeriod
 *
 * DESCRIPTION: Remove an extra space after a period.
 *
 ******************************************************************************/

void
AsRemoveSpacesAfterPeriod (
    char                    *FileBuffer,
    char                    *Filename)
{
    int                     ReplaceCount = 0;
    char                    *Possible;


    Possible = FileBuffer;
    while (Possible)
    {
        Possible = strstr (Possible, ".  ");
        if (Possible)
        {
            if ((*(Possible -1) == '.')  ||
                (*(Possible -1) == '\"') ||
                (*(Possible -1) == '\n'))
            {
                Possible += 3;
                continue;
            }

            Possible = AsReplaceData (Possible, 3, ". ", 2);
            ReplaceCount++;
        }
    }

    if (ReplaceCount)
    {
        printf ("Removed %d extra blanks after a period: %s\n",
            ReplaceCount, Filename);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsMatchExactWord
 *
 * DESCRIPTION: Check previous and next characters for whitespace
 *
 ******************************************************************************/

BOOLEAN
AsMatchExactWord (
    char                    *Word,
    UINT32                  WordLength)
{
    char                    NextChar;
    char                    PrevChar;


    NextChar = Word[WordLength];
    PrevChar = * (Word -1);

    if (isalnum ((int) NextChar) ||
        (NextChar == '_')  ||
        isalnum ((int) PrevChar) ||
        (PrevChar == '_'))
    {
        return (FALSE);
    }

    return (TRUE);
}


/******************************************************************************
 *
 * FUNCTION:    AsPrint
 *
 * DESCRIPTION: Common formatted print
 *
 ******************************************************************************/

void
AsPrint (
    char                    *Message,
    UINT32                  Count,
    char                    *Filename)
{

    if (Gbl_QuietMode)
    {
        return;
    }

    printf ("-- %4u %28.28s : %s\n", Count, Message, Filename);
}


/******************************************************************************
 *
 * FUNCTION:    AsTrimLines
 *
 * DESCRIPTION: Remove extra blanks from the end of source lines. Does not
 *              check for tabs.
 *
 ******************************************************************************/

void
AsTrimLines (
    char                    *Buffer,
    char                    *Filename)
{
    char                    *SubBuffer = Buffer;
    char                    *StartWhiteSpace = NULL;
    UINT32                  SpaceCount = 0;


    while (*SubBuffer)
    {
        while (*SubBuffer != '\n')
        {
            if (!*SubBuffer)
            {
                goto Exit;
            }

            if (*SubBuffer == ' ')
            {
                if (!StartWhiteSpace)
                {
                    StartWhiteSpace = SubBuffer;
                }
            }
            else
            {
                StartWhiteSpace = NULL;
            }

            SubBuffer++;
        }

        if (StartWhiteSpace)
        {
            SpaceCount += (SubBuffer - StartWhiteSpace);

            /* Remove the spaces */

            SubBuffer = AsRemoveData (StartWhiteSpace, SubBuffer);
            StartWhiteSpace = NULL;
        }

        SubBuffer++;
    }


Exit:
    if (SpaceCount)
    {
        Gbl_MadeChanges = TRUE;
        AsPrint ("Extraneous spaces removed", SpaceCount, Filename);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsTrimWhitespace
 *
 * DESCRIPTION: Remove "excess" blank lines - any more than 2 blank lines.
 *              this can happen during the translation when lines are removed.
 *
 ******************************************************************************/

void
AsTrimWhitespace (
    char                    *Buffer)
{
    char                    *SubBuffer;
    int                     ReplaceCount = 1;


    while (ReplaceCount)
    {
        ReplaceCount = AsReplaceString ("\n\n\n\n", "\n\n\n",
            REPLACE_SUBSTRINGS, Buffer);
    }

    /*
     * Check for exactly one blank line after the copyright header
     */

    /* Find the header */

    SubBuffer = strstr (Buffer, HeaderBegin);
    if (!SubBuffer)
    {
        return;
    }

    /* Find the end of the header */

    SubBuffer = strstr (SubBuffer, "*/");
    SubBuffer = AsSkipPastChar (SubBuffer, '\n');

    /* Replace a double blank line with a single */

    if (!strncmp (SubBuffer, "\n\n", 2))
    {
        AsReplaceData (SubBuffer, 2, "\n", 1);
        AcpiOsPrintf ("Found multiple blank lines after copyright\n");
    }

    /* If no blank line after header, insert one */

    else if (*SubBuffer != '\n')
    {
        AsInsertData (SubBuffer, "\n", 1);
        AcpiOsPrintf ("Inserted blank line after copyright\n");
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsReplaceHeader
 *
 * DESCRIPTION: Replace the default Intel legal header with a new header
 *
 ******************************************************************************/

void
AsReplaceHeader (
    char                    *Buffer,
    char                    *NewHeader)
{
    char                    *SubBuffer;
    char                    *TokenEnd;


    /* Find the original header */

    SubBuffer = strstr (Buffer, HeaderBegin);
    if (!SubBuffer)
    {
        return;
    }

    /* Find the end of the original header */

    TokenEnd = strstr (SubBuffer, "*/");
    TokenEnd = AsSkipPastChar (TokenEnd, '\n');

    /* Delete old header, insert new one */

    AsReplaceData (SubBuffer, TokenEnd - SubBuffer,
        NewHeader, strlen (NewHeader));
}


/******************************************************************************
 *
 * FUNCTION:    AsDoSpdxHeader
 *
 * DESCRIPTION: Replace the default Intel legal header with a new header
 *
 ******************************************************************************/

void
AsDoSpdxHeader (
    char                    *Buffer,
    char                    *SpdxHeader)
{
    char                    *SubBuffer;


    /* Place an SPDX header at the very top */

    AsReplaceData (Buffer, 0,
        SpdxHeader, strlen (SpdxHeader));

    /* Place an Intel copyright notice in the module header */

    SubBuffer = strstr (Buffer, MODULE_HEADER_END);
    if (!SubBuffer)
    {
        return;
    }

    AsReplaceData (SubBuffer, strlen (MODULE_HEADER_END),
        CopyRightHeaderEnd, strlen (CopyRightHeaderEnd));
}

/******************************************************************************
 *
 * FUNCTION:    AsReplaceString
 *
 * DESCRIPTION: Replace all instances of a target string with a replacement
 *              string. Returns count of the strings replaced.
 *
 ******************************************************************************/

int
AsReplaceString (
    char                    *Target,
    char                    *Replacement,
    UINT8                   Type,
    char                    *Buffer)
{
    char                    *SubString1;
    char                    *SubString2;
    char                    *SubBuffer;
    int                     TargetLength;
    int                     ReplacementLength;
    int                     ReplaceCount = 0;


    TargetLength = strlen (Target);
    ReplacementLength = strlen (Replacement);

    SubBuffer = Buffer;
    SubString1 = Buffer;

    while (SubString1)
    {
        /* Find the target string */

        SubString1 = strstr (SubBuffer, Target);
        if (!SubString1)
        {
            return (ReplaceCount);
        }

        /*
         * Check for translation escape string -- means to ignore
         * blocks of code while replacing
         */
        if (Gbl_IgnoreTranslationEscapes)
        {
            SubString2 = NULL;
        }
        else
        {
            SubString2 = strstr (SubBuffer, AS_START_IGNORE);
        }

        if ((SubString2) &&
            (SubString2 < SubString1))
        {
            /* Find end of the escape block starting at "Substring2" */

            SubString2 = strstr (SubString2, AS_STOP_IGNORE);
            if (!SubString2)
            {
                /* Didn't find terminator */

                return (ReplaceCount);
            }

            /* Move buffer to end of escape block and continue */

            SubBuffer = SubString2;
        }

        /* Do the actual replace if the target was found */

        else
        {
            if ((Type & REPLACE_MASK) == REPLACE_WHOLE_WORD)
            {
                if (!AsMatchExactWord (SubString1, TargetLength))
                {
                    SubBuffer = SubString1 + 1;
                    continue;
                }
            }

            SubBuffer = AsReplaceData (SubString1, TargetLength,
                Replacement, ReplacementLength);

            if ((Type & EXTRA_INDENT_C) &&
                (!Gbl_StructDefs))
            {
                SubBuffer = AsInsertData (SubBuffer, "        ", 8);
            }

            ReplaceCount++;
        }
    }

    return (ReplaceCount);
}


/******************************************************************************
 *
 * FUNCTION:    AsConvertToLineFeeds
 *
 * DESCRIPTION: Convert all CR/LF pairs to LF only.
 *
 ******************************************************************************/

void
AsConvertToLineFeeds (
    char                    *Buffer)
{
    char                    *SubString;
    char                    *SubBuffer;


    SubBuffer = Buffer;
    SubString = Buffer;

    while (SubString)
    {
        /* Find the target string */

        SubString = strstr (SubBuffer, "\r\n");
        if (!SubString)
        {
            return;
        }

        SubBuffer = AsReplaceData (SubString, 1, NULL, 0);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsInsertCarriageReturns
 *
 * DESCRIPTION: Convert lone LFs to CR/LF pairs.
 *
 ******************************************************************************/

void
AsInsertCarriageReturns (
    char                    *Buffer)
{
    char                    *SubString;
    char                    *SubBuffer;


    SubBuffer = Buffer;
    SubString = Buffer;

    while (SubString)
    {
        /* Find the target string */

        SubString = strstr (SubBuffer, "\n");
        if (!SubString)
        {
            return;
        }

        SubBuffer = AsInsertData (SubString, "\r", 1);
        SubBuffer += 1;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsBracesOnSameLine
 *
 * DESCRIPTION: Move opening braces up to the same line as an if, for, else,
 *              or while statement (leave function opening brace on separate
 *              line).
 *
 ******************************************************************************/

void
AsBracesOnSameLine (
    char                    *Buffer)
{
    char                    *SubBuffer = Buffer;
    char                    *Beginning;
    char                    *StartOfThisLine;
    char                    *Next;
    BOOLEAN                 BlockBegin = TRUE;


    while (*SubBuffer)
    {
        /* Ignore comments */

        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*'))
        {
            SubBuffer = strstr (SubBuffer, "*/");
            if (!SubBuffer)
            {
                return;
            }

            SubBuffer += 2;
            continue;
        }

        /* Ignore quoted strings */

        if (*SubBuffer == '\"')
        {
            SubBuffer++;
            SubBuffer = AsSkipPastChar (SubBuffer, '\"');
            if (!SubBuffer)
            {
                return;
            }
        }

        if (!strncmp ("\n}", SubBuffer, 2))
        {
            /*
             * A newline followed by a closing brace closes a function
             * or struct or initializer block
             */
            BlockBegin = TRUE;
        }

        /*
         * Move every standalone brace up to the previous line
         * Check for digit will ignore initializer lists surrounded by braces.
         * This will work until we we need more complex detection.
         */
        if ((*SubBuffer == '{') && !isdigit ((int) SubBuffer[1]))
        {
            if (BlockBegin)
            {
                BlockBegin = FALSE;
            }
            else
            {
                /*
                 * Backup to previous non-whitespace
                 */
                Beginning = SubBuffer - 1;
                while ((*Beginning == ' ')   ||
                       (*Beginning == '\n'))
                {
                    Beginning--;
                }

                StartOfThisLine = Beginning;
                while (*StartOfThisLine != '\n')
                {
                    StartOfThisLine--;
                }

                /*
                 * Move the brace up to the previous line, UNLESS:
                 *
                 * 1) There is a conditional compile on the line (starts with '#')
                 * 2) Previous line ends with an '=' (Start of initializer block)
                 * 3) Previous line ends with a comma (part of an init list)
                 * 4) Previous line ends with a backslash (part of a macro)
                 */
                if ((StartOfThisLine[1] != '#') &&
                    (*Beginning != '\\') &&
                    (*Beginning != '/') &&
                    (*Beginning != '{') &&
                    (*Beginning != '=') &&
                    (*Beginning != ','))
                {
                    Beginning++;
                    SubBuffer++;

                    Gbl_MadeChanges = TRUE;

#ifdef ADD_EXTRA_WHITESPACE
                    AsReplaceData (Beginning, SubBuffer - Beginning, " {\n", 3);
#else
                    /* Find non-whitespace start of next line */

                    Next = SubBuffer + 1;
                    while ((*Next == ' ')   ||
                           (*Next == '\t'))
                    {
                        Next++;
                    }

                    /* Find non-whitespace start of this line */

                    StartOfThisLine++;
                    while ((*StartOfThisLine == ' ')   ||
                           (*StartOfThisLine == '\t'))
                    {
                        StartOfThisLine++;
                    }

                    /*
                     * Must be a single-line comment to need more whitespace
                     * Even then, we don't need more if the previous statement
                     * is an "else".
                     */
                    if ((Next[0] == '/')  &&
                        (Next[1] == '*')  &&
                        (Next[2] != '\n') &&

                        (!strncmp (StartOfThisLine, "else if", 7)     ||
                         !strncmp (StartOfThisLine, "else while", 10) ||
                          strncmp (StartOfThisLine, "else", 4)))
                    {
                        AsReplaceData (Beginning, SubBuffer - Beginning, " {\n", 3);
                    }
                    else
                    {
                        AsReplaceData (Beginning, SubBuffer - Beginning, " {", 2);
                    }
#endif
                }
            }
        }

        SubBuffer++;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsTabify4
 *
 * DESCRIPTION: Convert the text to tabbed text. Alignment of text is
 *              preserved.
 *
 ******************************************************************************/

void
AsTabify4 (
    char                    *Buffer)
{
    char                    *SubBuffer = Buffer;
    char                    *NewSubBuffer;
    UINT32                  SpaceCount = 0;
    UINT32                  Column = 0;


    while (*SubBuffer)
    {
        if (*SubBuffer == '\n')
        {
            Column = 0;
        }
        else
        {
            Column++;
        }

        /* Ignore comments */

        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*'))
        {
            SubBuffer = strstr (SubBuffer, "*/");
            if (!SubBuffer)
            {
                return;
            }

            SubBuffer += 2;
            continue;
        }

        /* Ignore quoted strings */

        if (*SubBuffer == '\"')
        {
            SubBuffer++;
            SubBuffer = AsSkipPastChar (SubBuffer, '\"');
            if (!SubBuffer)
            {
                return;
            }
            SpaceCount = 0;
        }

        if (*SubBuffer == ' ')
        {
            SpaceCount++;

            if (SpaceCount >= 4)
            {
                SpaceCount = 0;

                NewSubBuffer = (SubBuffer + 1) - 4;
                *NewSubBuffer = '\t';
                NewSubBuffer++;

                /* Remove the spaces */

                SubBuffer = AsRemoveData (NewSubBuffer, SubBuffer + 1);
            }

            if ((Column % 4) == 0)
            {
                SpaceCount = 0;
            }
        }
        else
        {
            SpaceCount = 0;
        }

        SubBuffer++;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsTabify8
 *
 * DESCRIPTION: Convert the text to tabbed text. Alignment of text is
 *              preserved.
 *
 ******************************************************************************/

void
AsTabify8 (
    char                    *Buffer)
{
    char                    *SubBuffer = Buffer;
    char                    *NewSubBuffer;
    char                    *CommentEnd = NULL;
    UINT32                  SpaceCount = 0;
    UINT32                  TabCount = 0;
    UINT32                  LastLineTabCount = 0;
    UINT32                  LastLineColumnStart = 0;
    UINT32                  ThisColumnStart = 0;
    UINT32                  ThisTabCount =  0;
    char                    *FirstNonBlank = NULL;


    while (*SubBuffer)
    {
        if (*SubBuffer == '\n')
        {
            /* This is a standalone blank line */

            FirstNonBlank = NULL;
            SpaceCount = 0;
            TabCount = 0;
            SubBuffer++;
            continue;
        }

        if (!FirstNonBlank)
        {
            /* Find the first non-blank character on this line */

            FirstNonBlank = SubBuffer;
            while (*FirstNonBlank == ' ')
            {
                FirstNonBlank++;
            }

            /*
             * This mechanism limits the difference in tab counts from
             * line to line. It helps avoid the situation where a second
             * continuation line (which was indented correctly for tabs=4) would
             * get indented off the screen if we just blindly converted to tabs.
             */
            ThisColumnStart = FirstNonBlank - SubBuffer;

            if (LastLineTabCount == 0)
            {
                ThisTabCount = 0;
            }
            else if (ThisColumnStart == LastLineColumnStart)
            {
                ThisTabCount = LastLineTabCount -1;
            }
            else
            {
                ThisTabCount = LastLineTabCount + 1;
            }
        }

        /* Check if we are in a comment */

        if ((SubBuffer[0] == '*') &&
            (SubBuffer[1] == '/'))
        {
            SpaceCount = 0;
            SubBuffer += 2;

            if (*SubBuffer == '\n')
            {
                if (TabCount > 0)
                {
                    LastLineTabCount = TabCount;
                    TabCount = 0;
                }

                FirstNonBlank = NULL;
                LastLineColumnStart = ThisColumnStart;
                SubBuffer++;
            }

            continue;
        }

        /* Check for comment open */

        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*'))
        {
            /* Find the end of the comment, it must exist */

            CommentEnd = strstr (SubBuffer, "*/");
            if (!CommentEnd)
            {
                return;
            }

            /* Toss the rest of this line or single-line comment */

            while ((SubBuffer < CommentEnd) &&
                   (*SubBuffer != '\n'))
            {
                SubBuffer++;
            }

            if (*SubBuffer == '\n')
            {
                if (TabCount > 0)
                {
                    LastLineTabCount = TabCount;
                    TabCount = 0;
                }

                FirstNonBlank = NULL;
                LastLineColumnStart = ThisColumnStart;
            }

            SpaceCount = 0;
            continue;
        }

        /* Ignore quoted strings */

        if ((!CommentEnd) && (*SubBuffer == '\"'))
        {
            SubBuffer++;
            SubBuffer = AsSkipPastChar (SubBuffer, '\"');
            if (!SubBuffer)
            {
                return;
            }

            SpaceCount = 0;
        }

        if (*SubBuffer != ' ')
        {
            /* Not a space, skip to end of line */

            SubBuffer = AsSkipUntilChar (SubBuffer, '\n');
            if (!SubBuffer)
            {
                return;
            }
            if (TabCount > 0)
            {
                LastLineTabCount = TabCount;
                TabCount = 0;
            }

            FirstNonBlank = NULL;
            LastLineColumnStart = ThisColumnStart;
            SpaceCount = 0;
        }
        else
        {
            /* Another space */

            SpaceCount++;

            if (SpaceCount >= 4)
            {
                /* Replace this group of spaces with a tab character */

                SpaceCount = 0;

                NewSubBuffer = SubBuffer - 3;

                if (TabCount <= ThisTabCount ? (ThisTabCount +1) : 0)
                {
                    *NewSubBuffer = '\t';
                    NewSubBuffer++;
                    SubBuffer++;
                    TabCount++;
                }

                /* Remove the spaces */

                SubBuffer = AsRemoveData (NewSubBuffer, SubBuffer);
                continue;
            }
        }

        SubBuffer++;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsCountLines
 *
 * DESCRIPTION: Count the number of lines in the input buffer. Also count
 *              the number of long lines (lines longer than 80 chars).
 *
 ******************************************************************************/

static UINT32
AsCountLines (
    char                    *Buffer,
    char                    *Filename)
{
    char                    *SubBuffer = Buffer;
    char                    *EndOfLine;
    UINT32                  LineCount = 0;
    UINT32                  LongLineCount = 0;


    while (*SubBuffer)
    {
        EndOfLine = AsSkipUntilChar (SubBuffer, '\n');
        if (!EndOfLine)
        {
            Gbl_TotalLines += LineCount;
            return (LineCount);
        }

        if ((EndOfLine - SubBuffer) > 80)
        {
            LongLineCount++;
            VERBOSE_PRINT (("long: %.80s\n", SubBuffer));
        }

        LineCount++;
        SubBuffer = EndOfLine + 1;
    }

    if (LongLineCount)
    {
        VERBOSE_PRINT (("%u Lines longer than 80 found in %s\n",
            LongLineCount, Filename));

        Gbl_LongLines += LongLineCount;
    }

    Gbl_TotalLines += LineCount;
    return (LineCount);
}


/******************************************************************************
 *
 * FUNCTION:    AsCountTabs
 *
 * DESCRIPTION: Simply count the number of tabs in the input file buffer
 *
 ******************************************************************************/

void
AsCountTabs (
    char                    *Buffer,
    char                    *Filename)
{
    UINT32                  i;
    UINT32                  TabCount = 0;


    for (i = 0; Buffer[i]; i++)
    {
        if (Buffer[i] == '\t')
        {
            TabCount++;
        }
    }

    if (TabCount)
    {
        AsPrint ("Tabs found", TabCount, Filename);
        Gbl_Tabs += TabCount;
    }

    AsCountLines (Buffer, Filename);
}


/******************************************************************************
 *
 * FUNCTION:    AsCountSourceLines
 *
 * DESCRIPTION: Count the number of C source lines. Defined by 1) not a
 *              comment, and 2) not a blank line.
 *
 ******************************************************************************/

void
AsCountSourceLines (
    char                    *Buffer,
    char                    *Filename)
{
    char                    *SubBuffer = Buffer;
    UINT32                  LineCount = 0;
    UINT32                  WhiteCount = 0;
    UINT32                  CommentCount = 0;


    while (*SubBuffer)
    {
        /* Detect comments (// comments are not used, non-ansii) */

        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*'))
        {
            SubBuffer += 2;

            /* First line of multi-line comment is often just whitespace */

            if (SubBuffer[0] == '\n')
            {
                WhiteCount++;
                SubBuffer++;
            }
            else
            {
                CommentCount++;
            }

            /* Find end of comment */

            while (SubBuffer[0] && SubBuffer[1] &&
                !(((SubBuffer[0] == '*') &&
                    (SubBuffer[1] == '/'))))
            {
                if (SubBuffer[0] == '\n')
                {
                    CommentCount++;
                }

                SubBuffer++;
            }
        }

        /* A linefeed followed by a non-linefeed is a valid source line */

        else if ((SubBuffer[0] == '\n') &&
                 (SubBuffer[1] != '\n'))
        {
            LineCount++;
        }

        /* Two back-to-back linefeeds indicate a whitespace line */

        else if ((SubBuffer[0] == '\n') &&
                 (SubBuffer[1] == '\n'))
        {
            WhiteCount++;
        }

        SubBuffer++;
    }

    /* Adjust comment count for legal header */

    if (Gbl_HeaderSize < CommentCount)
    {
        CommentCount -= Gbl_HeaderSize;
        Gbl_HeaderLines += Gbl_HeaderSize;
    }

    Gbl_SourceLines += LineCount;
    Gbl_WhiteLines += WhiteCount;
    Gbl_CommentLines += CommentCount;

    VERBOSE_PRINT (("%u Comment %u White %u Code %u Lines in %s\n",
        CommentCount, WhiteCount, LineCount,
        LineCount + WhiteCount + CommentCount, Filename));
}


/******************************************************************************
 *
 * FUNCTION:    AsInsertPrefix
 *
 * DESCRIPTION: Insert struct or union prefixes
 *
 ******************************************************************************/

void
AsInsertPrefix (
    char                    *Buffer,
    char                    *Keyword,
    UINT8                   Type)
{
    char                    *SubString;
    char                    *SubBuffer;
    char                    *EndKeyword;
    int                     InsertLength;
    char                    *InsertString;
    int                     TrailingSpaces;
    char                    LowerKeyword[128];
    int                     KeywordLength;
    char                    *LineStart;
    BOOLEAN                 FoundPrefix;


    switch (Type)
    {
    case SRC_TYPE_STRUCT:

        InsertString = "struct ";
        break;

    case SRC_TYPE_UNION:

        InsertString = "union ";
        break;

    default:

        return;
    }

    strcpy (LowerKeyword, Keyword);
    AcpiUtStrlwr (LowerKeyword);

    SubBuffer = Buffer;
    SubString = Buffer;
    InsertLength = strlen (InsertString);
    KeywordLength = strlen (Keyword);


    while (SubString)
    {
        /* Find an instance of the keyword */

        SubString = strstr (SubBuffer, LowerKeyword);
        if (!SubString)
        {
            return;
        }

        SubBuffer = SubString;

        /* Must be standalone word, not a substring */

        if (AsMatchExactWord (SubString, KeywordLength))
        {
            /* Make sure the keyword isn't already prefixed with the insert */

            /* We find the beginning of the line and try to find the InsertString
             * from LineStart up to SubBuffer (our keyword). If it's not there,
             * we assume it doesn't have a prefix; this is a limitation, as having
             * a keyword on another line is absolutely valid C.
             */

            LineStart = SubString;
            FoundPrefix = FALSE;

            /* Find the start of the line */

            while (LineStart > Buffer)
            {
                if (*LineStart == '\n')
                {
                    LineStart++;
                    break;
                }

                LineStart--;
            }

            /* Try to find InsertString from the start of the line up to SubBuffer */
            /* Note that this algorithm is a bit naive. */

            while (SubBuffer > LineStart)
            {
                if (*LineStart != *InsertString)
                {
                    LineStart++;
                    continue;
                }

                if (strncmp (LineStart++, InsertString, InsertLength))
                {
                    continue;
                }

                FoundPrefix = TRUE;
                LineStart += InsertLength - 1;

                /* Now check if there's non-whitespace between InsertString and SubBuffer, as that
                 * means it's not a valid prefix in this case. */

                while (LineStart != SubBuffer)
                {
                    if (!strchr (" \t\r\n", *LineStart))
                    {
                        /* We found non-whitespace while traversing up to SubBuffer,
                         * so this isn't a prefix.
                         */
                        FoundPrefix = FALSE;
                        break;
                    }

                    LineStart++;
                }
            }

            if (FoundPrefix)
            {
                /* Add spaces if not already at the end-of-line */

                if (*(SubBuffer + KeywordLength) != '\n')
                {
                    /* Already present, add spaces after to align structure members */

#if 0
/* ONLY FOR C FILES */
                    AsInsertData (SubBuffer + KeywordLength, "        ", 8);
#endif
                }
                goto Next;
            }

            /* Make sure the keyword isn't at the end of a struct/union */
            /* Note: This code depends on a single space after the brace */

            if (*(SubString - 2) == '}')
            {
                goto Next;
            }

            /* Prefix the keyword with the insert string */

            Gbl_MadeChanges = TRUE;

            /* Is there room for insertion */

            EndKeyword = SubString + strlen (LowerKeyword);

            TrailingSpaces = 0;
            while (EndKeyword[TrailingSpaces] == ' ')
            {
                TrailingSpaces++;
            }

            /*
             * Use "if (TrailingSpaces > 1)" if we want to ignore casts
             */
            SubBuffer = SubString + InsertLength;

            if (TrailingSpaces > InsertLength)
            {
                /* Insert the keyword */

                memmove (SubBuffer, SubString, KeywordLength);

                /* Insert the keyword */

                memmove (SubString, InsertString, InsertLength);
            }
            else
            {
                AsInsertData (SubString, InsertString, InsertLength);
            }
        }

Next:
        SubBuffer += KeywordLength;
    }
}

#ifdef ACPI_FUTURE_IMPLEMENTATION
/******************************************************************************
 *
 * FUNCTION:    AsTrimComments
 *
 * DESCRIPTION: Finds 3-line comments with only a single line of text
 *
 ******************************************************************************/

void
AsTrimComments (
    char                    *Buffer,
    char                    *Filename)
{
    char                    *SubBuffer = Buffer;
    char                    *Ptr1;
    char                    *Ptr2;
    UINT32                  LineCount;
    UINT32                  ShortCommentCount = 0;


    while (1)
    {
        /* Find comment open, within procedure level */

        SubBuffer = strstr (SubBuffer, "    /*");
        if (!SubBuffer)
        {
            goto Exit;
        }

        /* Find comment terminator */

        Ptr1 = strstr (SubBuffer, "*/");
        if (!Ptr1)
        {
            goto Exit;
        }

        /* Find next EOL (from original buffer) */

        Ptr2 = strstr (SubBuffer, "\n");
        if (!Ptr2)
        {
            goto Exit;
        }

        /* Ignore one-line comments */

        if (Ptr1 < Ptr2)
        {
            /* Normal comment, ignore and continue; */

            SubBuffer = Ptr2;
            continue;
        }

        /* Examine multi-line comment */

        LineCount = 1;
        while (Ptr1 > Ptr2)
        {
            /* Find next EOL */

            Ptr2++;
            Ptr2 = strstr (Ptr2, "\n");
            if (!Ptr2)
            {
                goto Exit;
            }

            LineCount++;
        }

        SubBuffer = Ptr1;

        if (LineCount <= 3)
        {
            ShortCommentCount++;
        }
    }


Exit:

    if (ShortCommentCount)
    {
        AsPrint ("Short Comments found", ShortCommentCount, Filename);
    }
}
#endif

#ifdef ACPI_UNUSED_FUNCTIONS
/******************************************************************************
 *
 * FUNCTION:    AsCheckAndSkipLiterals
 *
 * DESCRIPTION: Generic routine to skip comments and quoted string literals.
 *              Keeps a line count.
 *
 ******************************************************************************/

static char *
AsCheckAndSkipLiterals (
    char                    *Buffer,
    UINT32                  *TotalLines);


static char *
AsCheckAndSkipLiterals (
    char                    *Buffer,
    UINT32                  *TotalLines)
{
    UINT32                  NewLines = 0;
    char                    *SubBuffer = Buffer;
    char                    *LiteralEnd;


    /* Ignore comments */

    if ((SubBuffer[0] == '/') &&
        (SubBuffer[1] == '*'))
    {
        LiteralEnd = strstr (SubBuffer, "*/");
        SubBuffer += 2;     /* Get past comment opening */

        if (!LiteralEnd)
        {
            return (SubBuffer);
        }

        while (SubBuffer < LiteralEnd)
        {
            if (*SubBuffer == '\n')
            {
                NewLines++;
            }

            SubBuffer++;
        }

        SubBuffer += 2;     /* Get past comment close */
    }

    /* Ignore quoted strings */

    else if (*SubBuffer == '\"')
    {
        SubBuffer++;
        LiteralEnd = AsSkipPastChar (SubBuffer, '\"');
        if (!LiteralEnd)
        {
            return (SubBuffer);
        }
    }

    if (TotalLines)
    {
        (*TotalLines) += NewLines;
    }
    return (SubBuffer);
}
#endif
