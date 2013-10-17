/******************************************************************************
 *
 * Module Name: asconvrt - Source conversion code
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

#include "acpisrc.h"

/* Local prototypes */

char *
AsCheckAndSkipLiterals (
    char                    *Buffer,
    UINT32                  *TotalLines);

UINT32
AsCountLines (
    char                    *Buffer,
    char                    *Filename);

/* Opening signature of the Intel legal header */

char        *HeaderBegin = "/******************************************************************************\n *\n * 1. Copyright Notice";


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
 * FUNCTION:    AsCheckAndSkipLiterals
 *
 * DESCRIPTION: Generic routine to skip comments and quoted string literals.
 *              Keeps a line count.
 *
 ******************************************************************************/

char *
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


/******************************************************************************
 *
 * FUNCTION:    AsAsCheckForBraces
 *
 * DESCRIPTION: Check for an open brace after each if statement
 *
 ******************************************************************************/

void
AsCheckForBraces (
    char                    *Buffer,
    char                    *Filename)
{
    char                    *SubBuffer = Buffer;
    char                    *NextBrace;
    char                    *NextSemicolon;
    char                    *NextIf;
    UINT32                  TotalLines = 1;


    while (*SubBuffer)
    {

        SubBuffer = AsCheckAndSkipLiterals (SubBuffer, &TotalLines);

        if (*SubBuffer == '\n')
        {
            TotalLines++;
        }
        else if (!(strncmp (" if", SubBuffer, 3)))
        {
            SubBuffer += 2;
            NextBrace = strstr (SubBuffer, "{");
            NextSemicolon = strstr (SubBuffer, ";");
            NextIf = strstr (SubBuffer, " if");

            if ((!NextBrace) ||
               (NextSemicolon && (NextBrace > NextSemicolon)) ||
               (NextIf && (NextBrace > NextIf)))
            {
                Gbl_MissingBraces++;

                if (!Gbl_QuietMode)
                {
                    printf ("Missing braces for <if>, line %u: %s\n", TotalLines, Filename);
                }
            }
        }
        else if (!(strncmp (" else if", SubBuffer, 8)))
        {
            SubBuffer += 7;
            NextBrace = strstr (SubBuffer, "{");
            NextSemicolon = strstr (SubBuffer, ";");
            NextIf = strstr (SubBuffer, " if");

            if ((!NextBrace) ||
               (NextSemicolon && (NextBrace > NextSemicolon)) ||
               (NextIf && (NextBrace > NextIf)))
            {
                Gbl_MissingBraces++;

                if (!Gbl_QuietMode)
                {
                    printf ("Missing braces for <if>, line %u: %s\n", TotalLines, Filename);
                }
            }
        }
        else if (!(strncmp (" else", SubBuffer, 5)))
        {
            SubBuffer += 4;
            NextBrace = strstr (SubBuffer, "{");
            NextSemicolon = strstr (SubBuffer, ";");
            NextIf = strstr (SubBuffer, " if");

            if ((!NextBrace) ||
               (NextSemicolon && (NextBrace > NextSemicolon)) ||
               (NextIf && (NextBrace > NextIf)))
            {
                Gbl_MissingBraces++;

                if (!Gbl_QuietMode)
                {
                    printf ("Missing braces for <else>, line %u: %s\n", TotalLines, Filename);
                }
            }
        }

        SubBuffer++;
    }
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
    int                     ReplaceCount = 1;


    while (ReplaceCount)
    {
        ReplaceCount = AsReplaceString ("\n\n\n\n", "\n\n\n", REPLACE_SUBSTRINGS, Buffer);
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

    AsReplaceData (SubBuffer, TokenEnd - SubBuffer, NewHeader, strlen (NewHeader));
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

            SubBuffer = AsReplaceData (SubString1, TargetLength, Replacement, ReplacementLength);

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
 * DESCRIPTION:
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
    return;
}


/******************************************************************************
 *
 * FUNCTION:    AsInsertCarriageReturns
 *
 * DESCRIPTION:
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
    return;
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
    UINT32                  Column = 0;
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
            Column = 0;
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

        Column++;

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
            Column = 0;
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

UINT32
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
        VERBOSE_PRINT (("%u Lines longer than 80 found in %s\n", LongLineCount, Filename));
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
 * FUNCTION:    AsCountNonAnsiComments
 *
 * DESCRIPTION: Count the number of "//" comments. This type of comment is
 *              non-ANSI C.
 *
 ******************************************************************************/

void
AsCountNonAnsiComments (
    char                    *Buffer,
    char                    *Filename)
{
    char                    *SubBuffer = Buffer;
    UINT32                  CommentCount = 0;


    while (SubBuffer)
    {
        SubBuffer = strstr (SubBuffer, "//");
        if (SubBuffer)
        {
            CommentCount++;
            SubBuffer += 2;
        }
    }

    if (CommentCount)
    {
        AsPrint ("Non-ANSI Comments found", CommentCount, Filename);
        Gbl_NonAnsiComments += CommentCount;
    }
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
                CommentCount, WhiteCount, LineCount, LineCount+WhiteCount+CommentCount, Filename));
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
    AsStrlwr (LowerKeyword);

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

            if (!strncmp (SubString - InsertLength, InsertString, InsertLength))
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
