
/******************************************************************************
 *
 * Module Name: asremove - Source conversion - removal functions
 *
 *****************************************************************************/

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

#include "acpisrc.h"

/* Local prototypes */

void
AsRemoveStatement (
    char                    *Buffer,
    char                    *Keyword,
    UINT32                  Type);


/******************************************************************************
 *
 * FUNCTION:    AsRemoveStatement
 *
 * DESCRIPTION: Remove all statements that contain the given keyword.
 *              Limitations:  Removes text from the start of the line that
 *              contains the keyword to the next semicolon.  Currently
 *              doesn't ignore comments.
 *
 ******************************************************************************/

void
AsRemoveStatement (
    char                    *Buffer,
    char                    *Keyword,
    UINT32                  Type)
{
    char                    *SubString;
    char                    *SubBuffer;
    int                     KeywordLength;


    KeywordLength = strlen (Keyword);
    SubBuffer = Buffer;
    SubString = Buffer;


    while (SubString)
    {
        SubString = strstr (SubBuffer, Keyword);

        if (SubString)
        {
            SubBuffer = SubString;

            if ((Type == REPLACE_WHOLE_WORD) &&
                (!AsMatchExactWord (SubString, KeywordLength)))
            {
                SubBuffer++;
                continue;
            }

            /* Find start of this line */

            while (*SubString != '\n')
            {
                SubString--;
            }
            SubString++;

            /* Find end of this statement */

            SubBuffer = AsSkipPastChar (SubBuffer, ';');
            if (!SubBuffer)
            {
                return;
            }

            /* Find end of this line */

            SubBuffer = AsSkipPastChar (SubBuffer, '\n');
            if (!SubBuffer)
            {
                return;
            }

            /* If next line is blank, remove it too */

            if (*SubBuffer == '\n')
            {
                SubBuffer++;
            }

            /* Remove the lines */

            SubBuffer = AsRemoveData (SubString, SubBuffer);
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveConditionalCompile
 *
 * DESCRIPTION: Remove a "#ifdef" statement, and all text that it encompasses.
 *              Limitations: cannot handle nested ifdefs.
 *
 ******************************************************************************/

void
AsRemoveConditionalCompile (
    char                    *Buffer,
    char                    *Keyword)
{
    char                    *SubString;
    char                    *SubBuffer;
    char                    *IfPtr;
    char                    *EndifPtr;
    char                    *ElsePtr;
    char                    *Comment;
    int                     KeywordLength;


    KeywordLength = strlen (Keyword);
    SubBuffer = Buffer;
    SubString = Buffer;


    while (SubString)
    {
        SubBuffer = strstr (SubString, Keyword);
        if (!SubBuffer)
        {
            return;
        }

        /*
         * Check for translation escape string -- means to ignore
         * blocks of code while replacing
         */
        Comment = strstr (SubString, AS_START_IGNORE);

        if ((Comment) &&
            (Comment < SubBuffer))
        {
            SubString = strstr (Comment, AS_STOP_IGNORE);
            if (!SubString)
            {
                return;
            }

            SubString += 3;
            continue;
        }

        /* Check for ordinary comment */

        Comment = strstr (SubString, "/*");

        if ((Comment) &&
            (Comment < SubBuffer))
        {
            SubString = strstr (Comment, "*/");
            if (!SubString)
            {
                return;
            }

            SubString += 2;
            continue;
        }

        SubString = SubBuffer;
        if (!AsMatchExactWord (SubString, KeywordLength))
        {
            SubString++;
            continue;
        }

        /* Find start of this line */

        while (*SubString != '\n' && (SubString > Buffer))
        {
            SubString--;
        }
        SubString++;

        /* Find the "#ifxxxx" */

        IfPtr = strstr (SubString, "#if");
        if (!IfPtr)
        {
            return;
        }

        if (IfPtr > SubBuffer)
        {
            /* Not the right #if */

            SubString = SubBuffer + strlen (Keyword);
            continue;
        }

        /* Find closing #endif or #else */

        EndifPtr = strstr (SubBuffer, "#endif");
        if (!EndifPtr)
        {
            /* There has to be an #endif */

            return;
        }

        ElsePtr = strstr (SubBuffer, "#else");
        if ((ElsePtr) &&
            (EndifPtr > ElsePtr))
        {
            /* This #ifdef contains an #else clause */
            /* Find end of this line */

            SubBuffer = AsSkipPastChar (ElsePtr, '\n');
            if (!SubBuffer)
            {
                return;
            }

            /* Remove the #ifdef .... #else code */

            AsRemoveData (SubString, SubBuffer);

            /* Next, we will remove the #endif statement */

            EndifPtr = strstr (SubString, "#endif");
            if (!EndifPtr)
            {
                /* There has to be an #endif */

                return;
            }

            SubString = EndifPtr;
        }

        /* Remove the ... #endif part */
        /* Find end of this line */

        SubBuffer = AsSkipPastChar (EndifPtr, '\n');
        if (!SubBuffer)
        {
            return;
        }

        /* Remove the lines */

        SubBuffer = AsRemoveData (SubString, SubBuffer);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveMacro
 *
 * DESCRIPTION: Remove every line that contains the keyword.  Does not
 *              skip comments.
 *
 ******************************************************************************/

void
AsRemoveMacro (
    char                    *Buffer,
    char                    *Keyword)
{
    char                    *SubString;
    char                    *SubBuffer;
    int                     NestLevel;


    SubBuffer = Buffer;
    SubString = Buffer;


    while (SubString)
    {
        SubString = strstr (SubBuffer, Keyword);

        if (SubString)
        {
            SubBuffer = SubString;

            /* Find start of the macro parameters */

            while (*SubString != '(')
            {
                SubString++;
            }
            SubString++;

            /* Remove the macro name and opening paren */

            SubString = AsRemoveData (SubBuffer, SubString);

            NestLevel = 1;
            while (*SubString)
            {
                if (*SubString == '(')
                {
                    NestLevel++;
                }
                else if (*SubString == ')')
                {
                    NestLevel--;
                }

                SubString++;

                if (NestLevel == 0)
                {
                    break;
                }
            }

            /* Remove the closing paren */

            SubBuffer = AsRemoveData (SubString-1, SubString);
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveLine
 *
 * DESCRIPTION: Remove every line that contains the keyword.  Does not
 *              skip comments.
 *
 ******************************************************************************/

void
AsRemoveLine (
    char                    *Buffer,
    char                    *Keyword)
{
    char                    *SubString;
    char                    *SubBuffer;


    SubBuffer = Buffer;
    SubString = Buffer;


    while (SubString)
    {
        SubString = strstr (SubBuffer, Keyword);

        if (SubString)
        {
            SubBuffer = SubString;

            /* Find start of this line */

            while (*SubString != '\n')
            {
                SubString--;
            }
            SubString++;

            /* Find end of this line */

            SubBuffer = AsSkipPastChar (SubBuffer, '\n');
            if (!SubBuffer)
            {
                return;
            }

            /* Remove the line */

            SubBuffer = AsRemoveData (SubString, SubBuffer);
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsReduceTypedefs
 *
 * DESCRIPTION: Eliminate certain typedefs
 *
 ******************************************************************************/

void
AsReduceTypedefs (
    char                    *Buffer,
    char                    *Keyword)
{
    char                    *SubString;
    char                    *SubBuffer;
    int                     NestLevel;


    SubBuffer = Buffer;
    SubString = Buffer;


    while (SubString)
    {
        SubString = strstr (SubBuffer, Keyword);

        if (SubString)
        {
            /* Remove the typedef itself */

            SubBuffer = SubString + strlen ("typedef") + 1;
            SubBuffer = AsRemoveData (SubString, SubBuffer);

            /* Find the opening brace of the struct or union */

            while (*SubString != '{')
            {
                SubString++;
            }
            SubString++;

            /* Find the closing brace.  Handles nested braces */

            NestLevel = 1;
            while (*SubString)
            {
                if (*SubString == '{')
                {
                    NestLevel++;
                }
                else if (*SubString == '}')
                {
                    NestLevel--;
                }

                SubString++;

                if (NestLevel == 0)
                {
                    break;
                }
            }

            /* Remove an extra line feed if present */

            if (!strncmp (SubString - 3, "\n\n", 2))
            {
                *(SubString -2) = '}';
                SubString--;
            }

            /* Find the end of the typedef name */

            SubBuffer = AsSkipUntilChar (SubString, ';');

            /* And remove the typedef name */

            SubBuffer = AsRemoveData (SubString, SubBuffer);
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveEmptyBlocks
 *
 * DESCRIPTION: Remove any C blocks (e.g., if {}) that contain no code.  This
 *              can happen as a result of removing lines such as DEBUG_PRINT.
 *
 ******************************************************************************/

void
AsRemoveEmptyBlocks (
    char                    *Buffer,
    char                    *Filename)
{
    char                    *SubBuffer;
    char                    *BlockStart;
    BOOLEAN                 EmptyBlock = TRUE;
    BOOLEAN                 AnotherPassRequired = TRUE;
    UINT32                  BlockCount = 0;


    while (AnotherPassRequired)
    {
        SubBuffer = Buffer;
        AnotherPassRequired = FALSE;

        while (*SubBuffer)
        {
            if (*SubBuffer == '{')
            {
                BlockStart = SubBuffer;
                EmptyBlock = TRUE;

                SubBuffer++;
                while (*SubBuffer != '}')
                {
                    if ((*SubBuffer != ' ') &&
                        (*SubBuffer != '\n'))
                    {
                        EmptyBlock = FALSE;
                        break;
                    }
                    SubBuffer++;
                }

                if (EmptyBlock)
                {
                    /* Find start of the first line of the block */

                    while (*BlockStart != '\n')
                    {
                        BlockStart--;
                    }

                    /* Find end of the last line of the block */

                    SubBuffer = AsSkipUntilChar (SubBuffer, '\n');
                    if (!SubBuffer)
                    {
                        break;
                    }

                    /* Remove the block */

                    SubBuffer = AsRemoveData (BlockStart, SubBuffer);
                    BlockCount++;
                    AnotherPassRequired = TRUE;
                    continue;
                }
            }

            SubBuffer++;
        }
    }

    if (BlockCount)
    {
        Gbl_MadeChanges = TRUE;
        AsPrint ("Code blocks deleted", BlockCount, Filename);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveDebugMacros
 *
 * DESCRIPTION: Remove all "Debug" macros -- macros that produce debug output.
 *
 ******************************************************************************/

void
AsRemoveDebugMacros (
    char                    *Buffer)
{
    AsRemoveConditionalCompile (Buffer, "ACPI_DEBUG_OUTPUT");

    AsRemoveStatement (Buffer, "ACPI_DEBUG_PRINT",      REPLACE_WHOLE_WORD);
    AsRemoveStatement (Buffer, "ACPI_DEBUG_PRINT_RAW",  REPLACE_WHOLE_WORD);
    AsRemoveStatement (Buffer, "DEBUG_EXEC",            REPLACE_WHOLE_WORD);
    AsRemoveStatement (Buffer, "FUNCTION_ENTRY",        REPLACE_WHOLE_WORD);
    AsRemoveStatement (Buffer, "PROC_NAME",             REPLACE_WHOLE_WORD);
    AsRemoveStatement (Buffer, "FUNCTION_TRACE",        REPLACE_SUBSTRINGS);
    AsRemoveStatement (Buffer, "DUMP_",                 REPLACE_SUBSTRINGS);

    AsReplaceString ("return_VOID",         "return", REPLACE_WHOLE_WORD, Buffer);
    AsReplaceString ("return_PTR",          "return", REPLACE_WHOLE_WORD, Buffer);
    AsReplaceString ("return_ACPI_STATUS",  "return", REPLACE_WHOLE_WORD, Buffer);
    AsReplaceString ("return_acpi_status",  "return", REPLACE_WHOLE_WORD, Buffer);
    AsReplaceString ("return_VALUE",        "return", REPLACE_WHOLE_WORD, Buffer);
}


