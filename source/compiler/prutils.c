/******************************************************************************
 *
 * Module Name: prutils - Preprocessor utilities
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

#include "aslcompiler.h"

#define _COMPONENT          ASL_PREPROCESSOR
        ACPI_MODULE_NAME    ("prutils")


/******************************************************************************
 *
 * FUNCTION:    PrGetNextToken
 *
 * PARAMETERS:  Buffer              - Current line buffer
 *              MatchString         - String with valid token delimiters
 *              Next                - Set to next possible token in buffer
 *
 * RETURN:      Next token (null-terminated). Modifies the input line.
 *              Remainder of line is stored in *Next.
 *
 * DESCRIPTION: Local implementation of strtok() with local storage for the
 *              next pointer. Not only thread-safe, but allows multiple
 *              parsing of substrings such as expressions.
 *
 *****************************************************************************/

char *
PrGetNextToken (
    char                    *Buffer,
    char                    *MatchString,
    char                    **Next)
{
    char                    *TokenStart;


    if (!Buffer)
    {
        /* Use Next if it is valid */

        Buffer = *Next;
        if (!(*Next))
        {
            return (NULL);
        }
    }

    /* Skip any leading delimiters */

    while (*Buffer)
    {
        if (strchr (MatchString, *Buffer))
        {
            Buffer++;
        }
        else
        {
            break;
        }
    }

    /* Anything left on the line? */

    if (!(*Buffer))
    {
        *Next = NULL;
        return (NULL);
    }

    TokenStart = Buffer;

    /* Find the end of this token */

    while (*Buffer)
    {
        if (strchr (MatchString, *Buffer))
        {
            *Buffer = 0;
            *Next = Buffer+1;
            if (!**Next)
            {
                *Next = NULL;
            }

            return (TokenStart);
        }

        Buffer++;
    }

    *Next = NULL;
    return (TokenStart);
}


/*******************************************************************************
 *
 * FUNCTION:    PrError
 *
 * PARAMETERS:  Level               - Seriousness (Warning/error, etc.)
 *              MessageId           - Index into global message buffer
 *              Column              - Column in current line
 *
 * RETURN:      None
 *
 * DESCRIPTION: Preprocessor error reporting. Front end to AslCommonError2
 *
 ******************************************************************************/

void
PrError (
    UINT8                   Level,
    UINT16                  MessageId,
    UINT32                  Column)
{
#if 0
    AcpiOsPrintf ("%s (%u) : %s", AslGbl_Files[ASL_FILE_INPUT].Filename,
        AslGbl_CurrentLineNumber, AslGbl_CurrentLineBuffer);
#endif


    if (Column > 120)
    {
        Column = 0;
    }

    /* TBD: Need Logical line number? */

    AslCommonError2 (Level, MessageId,
        AslGbl_CurrentLineNumber, Column,
        AslGbl_CurrentLineBuffer,
        AslGbl_Files[ASL_FILE_INPUT].Filename, "Preprocessor");

    AslGbl_PreprocessorError = TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    PrReplaceResizeSubstring
 *
 * PARAMETERS:  Args                - Struct containing name, offset & usecount
 *              Diff1               - Difference in lengths when new < old
 *              Diff2               - Difference in lengths when new > old
*               i                   - The curr. no. of iteration of replacement
 *              Token               - Substring that replaces Args->Name
 *
 * RETURN:      None
 *
 * DESCRIPTION: Advanced substring replacement in a string using resized buffer.
 *
 ******************************************************************************/

void
PrReplaceResizeSubstring(
    PR_MACRO_ARG            *Args,
    UINT32                  Diff1,
    UINT32                  Diff2,
    UINT32                  i,
    char                    *Token)
{
    UINT32                  b, PrevOffset;
    char                    *temp;
    char                    macro_sep[64];


    AslGbl_MacroTokenReplaceBuffer = (char *) realloc (AslGbl_MacroTokenReplaceBuffer,
        (2 * (strlen (AslGbl_MacroTokenBuffer))));

    strcpy (macro_sep, "~,() {}!*/%+-<>=&^|\"\t\n");

    /*
     * When the replacement argument (during invocation) length
     * < replaced parameter (in the macro function definition
     * and its expansion) length
     */
    if (Diff1 != 0)
    {
        /*
         * We save the offset value to reset it after replacing each
         * instance of each arg and setting the offset value to
         * the start of the arg to be replaced since it changes
         * with each iteration when arg length != token length
         */
        PrevOffset = Args->Offset[i];
        temp = strstr (AslGbl_MacroTokenBuffer, Args->Name);

ResetHere1:
        temp = strstr (temp, Args->Name);
        Args->Offset[i] = strlen (AslGbl_MacroTokenBuffer) -
            strlen (temp);
        if (Args->Offset[i] == 0)
        {
            goto JumpHere1;
        }
        if ((strchr (macro_sep, AslGbl_MacroTokenBuffer[(Args->Offset[i] - 1)])) &&
            (strchr (macro_sep, AslGbl_MacroTokenBuffer[(Args->Offset[i] + strlen (Args->Name))])))
        {
            Args->Offset[i] += 0;
        }
        else
        {
            temp += strlen (Args->Name);
            goto ResetHere1;
        }

        /*
         * For now, we simply set the extra char positions (generated
         * due to longer name replaced by shorter name) to whitespace
         * chars so it will be ignored during compilation
         */
JumpHere1:
        b = strlen (Token) + Args->Offset[i];
        memset (&AslGbl_MacroTokenBuffer[b], ' ', Diff1);

# if 0

    /* Work in progress as of 03/08/2023 - experimental 'if' block
     * to test code for removing extra whitespaces from the macro
     * replacement when replacement arg < replaced param
     */
        char Buff[8192];
        /* char* Replace; */
        /* Replace = Buff; */

        for (j = 0; j < strlen (AslGbl_MacroTokenBuffer); j++)
        {
            Buff[j] = AslGbl_MacroTokenBuffer[j];
        }
        Buff[strlen (AslGbl_MacroTokenBuffer)] = '\0';
        //fprintf (stderr, "Buff: %s\n", Buff);

        UINT32 len = strlen (Buff);

        for (j = 0; j < len; j++)
        {
            if (Buff[0] == ' ')
            {
                for (j = 0; j < (len - 1); j++)
                {
                    Buff[j] = Buff[j + 1];
                }
                Buff[j] = '\0';
                len--;
                j = -1;
                continue;
            }

            if (Buff[j] == ' ' && Buff[j + 1] == ' ')
            {
                for (k = 0; k < (len - 1); k++)
                {
                    Buff[j] = Buff[j + 1];
                }
                Buff[j] = '\0';
                len--;
                j--;
            }
        }
        //fprintf(stderr, "Buff: %s\n", Buff);

        for (k = 0; k < strlen (Buff); k++)
        {
            AslGbl_MacroTokenBuffer[k] = Buff[k];
        }
#endif

        PrReplaceData (
            &AslGbl_MacroTokenBuffer[Args->Offset[i]],
            strlen (Token), Token, strlen (Token));

        temp = NULL;
        Args->Offset[i] = PrevOffset;
    }

    /*
     * When the replacement argument (during invocation) length
     * > replaced parameter (in the macro function definition
     * and its expansion) length
     */
    else if (Diff2 != 0)
    {
        /* Doing the same thing with offset value as for prev case */

        PrevOffset = Args->Offset[i];
        temp = strstr (AslGbl_MacroTokenBuffer, Args->Name);

ResetHere2:
        temp = strstr (temp, Args->Name);
        Args->Offset[i] = strlen (AslGbl_MacroTokenBuffer) -
            strlen (temp);
        if (Args->Offset[i] == 0)
        {
            goto JumpHere2;
        }
        if ((strchr (macro_sep, AslGbl_MacroTokenBuffer[(Args->Offset[i] - 1)])) &&
            (strchr (macro_sep, AslGbl_MacroTokenBuffer[(Args->Offset[i] + strlen (Args->Name))])))
        {
            Args->Offset[i] += 0;
        }
        else
        {
            temp+= strlen (Args->Name);
            goto ResetHere2;
        }

        /*
         * We will need to allocate some extra space in our buffer to
         * accommodate the increase in the replacement string length
         * over the shorter outgoing arg string and do the replacement
         * at the correct offset value which is resetted every iteration
         */
JumpHere2:
        strncpy (AslGbl_MacroTokenReplaceBuffer, AslGbl_MacroTokenBuffer, Args->Offset[i]);
        strcat (AslGbl_MacroTokenReplaceBuffer, Token);
        strcat (AslGbl_MacroTokenReplaceBuffer, (AslGbl_MacroTokenBuffer +
            (Args->Offset[i] + strlen (Args->Name))));

        strcpy (AslGbl_MacroTokenBuffer, AslGbl_MacroTokenReplaceBuffer);

        temp = NULL;
        Args->Offset[i] = PrevOffset;
    }

    /*
     * When the replacement argument (during invocation) length =
     * replaced parameter (in the macro function definition and
     * its expansion) length
     */
    else
    {

        /*
         * We still need to reset the offset for each iteration even when
         * arg and param lengths are same since any macro func invocation
         * could use various cases for each separate arg-param pair
         */
        PrevOffset = Args->Offset[i];
        temp = strstr (AslGbl_MacroTokenBuffer, Args->Name);

ResetHere3:
        temp = strstr (temp, Args->Name);
        Args->Offset[i] = strlen (AslGbl_MacroTokenBuffer) -
            strlen (temp);
        if (Args->Offset[i] == 0)
        {
            goto JumpHere3;
        }
        if ((strchr (macro_sep, AslGbl_MacroTokenBuffer[(Args->Offset[i] - 1)])) &&
            (strchr (macro_sep, AslGbl_MacroTokenBuffer[(Args->Offset[i] + strlen (Args->Name))])))
        {
            Args->Offset[i] += 0;
        }
        else
        {
            temp += strlen (Args->Name);
            goto ResetHere3;
        }

JumpHere3:
        PrReplaceData (
            &AslGbl_MacroTokenBuffer[Args->Offset[i]],
            strlen (Args->Name), Token, strlen (Token));
        temp = NULL;
        Args->Offset[i] = PrevOffset;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    PrReplaceData
 *
 * PARAMETERS:  Buffer              - Original(target) buffer pointer
 *              LengthToRemove      - Length to be removed from target buffer
 *              BufferToAdd         - Data to be inserted into target buffer
 *              LengthToAdd         - Length of BufferToAdd
 *
 * RETURN:      Pointer to where the buffer is replaced with data
 *
 * DESCRIPTION: Generic buffer data replacement.
 *
 ******************************************************************************/

char *
PrReplaceData (
    char                    *Buffer,
    UINT32                  LengthToRemove,
    char                    *BufferToAdd,
    UINT32                  LengthToAdd)
{
    UINT32                  BufferLength;


    /* Buffer is a string, so the length must include the terminating zero */

    BufferLength = strlen (Buffer) + 1;

    if (LengthToRemove != LengthToAdd)
    {
        /*
         * Move some of the existing data
         * 1) If adding more bytes than removing, make room for the new data
         * 2) if removing more bytes than adding, delete the extra space
         */
        if (LengthToRemove > 0)
        {
            memmove ((Buffer + LengthToAdd), (Buffer + LengthToRemove),
                (BufferLength - LengthToRemove));
        }
    }


    /* Now we can move in the new data */

    if (LengthToAdd > 0)
    {
        memmove (Buffer, BufferToAdd, LengthToAdd);
    }
    return (Buffer + LengthToAdd);
}


/*******************************************************************************
 *
 * FUNCTION:    PrOpenIncludeFile
 *
 * PARAMETERS:  Filename            - Filename or pathname for include file
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Open an include file and push it on the input file stack.
 *
 ******************************************************************************/

FILE *
PrOpenIncludeFile (
    char                    *Filename,
    char                    *OpenMode,
    char                    **FullPathname)
{
    FILE                    *IncludeFile;
    ASL_INCLUDE_DIR         *NextDir;


    /* Start the actual include file on the next line */

    AslGbl_CurrentLineOffset++;

    /* Attempt to open the include file */
    /* If the file specifies an absolute path, just open it */

    if ((Filename[0] == '/')  ||
        (Filename[0] == '\\') ||
        (Filename[1] == ':'))
    {
        IncludeFile = PrOpenIncludeWithPrefix (
            "", Filename, OpenMode, FullPathname);
        if (!IncludeFile)
        {
            goto ErrorExit;
        }
        return (IncludeFile);
    }

    /*
     * The include filename is not an absolute path.
     *
     * First, search for the file within the "local" directory -- meaning
     * the same directory that contains the source file.
     *
     * Construct the file pathname from the global directory name.
     */
    IncludeFile = PrOpenIncludeWithPrefix (
        AslGbl_DirectoryPath, Filename, OpenMode, FullPathname);
    if (IncludeFile)
    {
        return (IncludeFile);
    }

    /*
     * Second, search for the file within the (possibly multiple)
     * directories specified by the -I option on the command line.
     */
    NextDir = AslGbl_IncludeDirList;
    while (NextDir)
    {
        IncludeFile = PrOpenIncludeWithPrefix (
            NextDir->Dir, Filename, OpenMode, FullPathname);
        if (IncludeFile)
        {
            return (IncludeFile);
        }

        NextDir = NextDir->Next;
    }

    /* We could not open the include file after trying very hard */

ErrorExit:
    sprintf (AslGbl_MainTokenBuffer, "%s, %s", Filename, strerror (errno));
    PrError (ASL_ERROR, ASL_MSG_INCLUDE_FILE_OPEN, 0);
    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenIncludeWithPrefix
 *
 * PARAMETERS:  PrefixDir       - Prefix directory pathname. Can be a zero
 *                                length string.
 *              Filename        - The include filename from the source ASL.
 *
 * RETURN:      Valid file descriptor if successful. Null otherwise.
 *
 * DESCRIPTION: Open an include file and push it on the input file stack.
 *
 ******************************************************************************/

FILE *
PrOpenIncludeWithPrefix (
    char                    *PrefixDir,
    char                    *Filename,
    char                    *OpenMode,
    char                    **FullPathname)
{
    FILE                    *IncludeFile;
    char                    *Pathname;


    /* Build the full pathname to the file */

    Pathname = FlMergePathnames (PrefixDir, Filename);

    DbgPrint (ASL_PARSE_OUTPUT, PR_PREFIX_ID
        "Include: Opening file - \"%s\"\n",
        AslGbl_CurrentLineNumber, Pathname);

    /* Attempt to open the file, push if successful */

    IncludeFile = fopen (Pathname, OpenMode);
    if (!IncludeFile)
    {
        return (NULL);
    }

    /* Push the include file on the open input file stack */

    PrPushInputFileStack (IncludeFile, Pathname);
    *FullPathname = Pathname;
    return (IncludeFile);
}


/*******************************************************************************
 *
 * FUNCTION:    AslPushInputFileStack
 *
 * PARAMETERS:  InputFile           - Open file pointer
 *              Filename            - Name of the file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Push the InputFile onto the file stack, and point the parser
 *              to this file. Called when an include file is successfully
 *              opened.
 *
 ******************************************************************************/

void
PrPushInputFileStack (
    FILE                    *InputFile,
    char                    *Filename)
{
    PR_FILE_NODE            *Fnode;


    AslGbl_HasIncludeFiles = TRUE;

    /* Save the current state in an Fnode */

    Fnode = UtLocalCalloc (sizeof (PR_FILE_NODE));

    Fnode->File = AslGbl_Files[ASL_FILE_INPUT].Handle;
    Fnode->Next = AslGbl_InputFileList;
    Fnode->Filename = AslGbl_Files[ASL_FILE_INPUT].Filename;
    Fnode->CurrentLineNumber = AslGbl_CurrentLineNumber;

    /* Push it on the stack */

    AslGbl_InputFileList = Fnode;

    DbgPrint (ASL_PARSE_OUTPUT, PR_PREFIX_ID
        "Push InputFile Stack: handle %p\n\n",
        AslGbl_CurrentLineNumber, InputFile);

    /* Reset the global line count and filename */

    AslGbl_Files[ASL_FILE_INPUT].Filename =
        UtLocalCacheCalloc (strlen (Filename) + 1);
    strcpy (AslGbl_Files[ASL_FILE_INPUT].Filename, Filename);

    AslGbl_Files[ASL_FILE_INPUT].Handle = InputFile;
    AslGbl_CurrentLineNumber = 1;

    /* Emit a new #line directive for the include file */

    FlPrintFile (ASL_FILE_PREPROCESSOR, "#line %u \"%s\"\n", 1, Filename);
}


/*******************************************************************************
 *
 * FUNCTION:    AslPopInputFileStack
 *
 * PARAMETERS:  None
 *
 * RETURN:      0 if a node was popped, -1 otherwise
 *
 * DESCRIPTION: Pop the top of the input file stack and point the parser to
 *              the saved parse buffer contained in the fnode. Also, set the
 *              global line counters to the saved values. This function is
 *              called when an include file reaches EOF.
 *
 ******************************************************************************/

BOOLEAN
PrPopInputFileStack (
    void)
{
    PR_FILE_NODE            *Fnode;


    Fnode = AslGbl_InputFileList;
    DbgPrint (ASL_PARSE_OUTPUT, "\n" PR_PREFIX_ID
        "Pop InputFile Stack, Fnode %p\n\n",
        AslGbl_CurrentLineNumber, Fnode);

    if (!Fnode)
    {
        return (FALSE);
    }

    /* Close the current include file */

    fclose (AslGbl_Files[ASL_FILE_INPUT].Handle);

    /* Update the top-of-stack */

    AslGbl_InputFileList = Fnode->Next;

    /* Reset global line counter and filename */

    AslGbl_Files[ASL_FILE_INPUT].Filename = Fnode->Filename;
    AslGbl_Files[ASL_FILE_INPUT].Handle = Fnode->File;
    AslGbl_CurrentLineNumber = Fnode->CurrentLineNumber;

    /* Emit a new #line directive after the include file */

    FlPrintFile (ASL_FILE_PREPROCESSOR, "#line %u \"%s\"\n",
        AslGbl_CurrentLineNumber, Fnode->Filename);

    /* All done with this node */

    ACPI_FREE (Fnode);
    return (TRUE);
}
