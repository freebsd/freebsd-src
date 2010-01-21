
/******************************************************************************
 *
 * Module Name: asremove - Source conversion - removal functions
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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


