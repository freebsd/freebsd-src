/******************************************************************************
 *
 * Module Name: aslsupport.l - Flex/lex scanner C support routines.
 *              NOTE: Included into aslcompile.l, not compiled by itself.
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


/* Configuration */

#define ASL_SPACES_PER_TAB      4

#define ASL_NORMAL_CHAR         0
#define ASL_ESCAPE_SEQUENCE     1
#define ASL_OCTAL_CONSTANT      2
#define ASL_HEX_CONSTANT        3


/* File node - used for "Include" operator file stack */

typedef struct asl_file_node
{
    FILE                    *File;
    UINT32                  CurrentLineNumber;
    YY_BUFFER_STATE         State;
    char                    *Filename;
    struct asl_file_node    *Next;

} ASL_FILE_NODE;

/* File stack for the "Include" operator (NOT #include operator) */

ASL_FILE_NODE               *Gbl_IncludeFileStack = NULL;


/*******************************************************************************
 *
 * FUNCTION:    AslDoLineDirective
 *
 * PARAMETERS:  None. Uses input() to access current source code line
 *
 * RETURN:      Updates global line number and filename
 *
 * DESCRIPTION: Handle #line directives emitted by the preprocessor.
 *
 * The #line directive is emitted by the preprocesser, and is used to
 * pass through line numbers from the original source code file to the
 * preprocessor output file (.i). This allows any compiler-generated
 * error messages to be displayed with the correct line number.
 *
 ******************************************************************************/

static void
AslDoLineDirective (
    void)
{
    int                     c;
    char                    *Token;
    UINT32                  LineNumber;
    char                    *Filename;
    UINT32                  i;


    /* Eat the entire line that contains the #line directive */

    Gbl_LineBufPtr = Gbl_CurrentLineBuffer;

    while ((c = input()) != '\n' && c != EOF)
    {
        *Gbl_LineBufPtr = c;
        Gbl_LineBufPtr++;
    }
    *Gbl_LineBufPtr = 0;

    /* First argument is the actual line number */

    Token = strtok (Gbl_CurrentLineBuffer, " ");
    if (!Token)
    {
        goto ResetAndExit;
    }

    /* First argument is the line number */

    LineNumber = (UINT32) UtDoConstant (Token);

    /* Emit the appropriate number of newlines */

    Gbl_CurrentColumn = 0;
    if (LineNumber > Gbl_CurrentLineNumber)
    {
        for (i = 0; i < (LineNumber - Gbl_CurrentLineNumber); i++)
        {
            FlWriteFile (ASL_FILE_SOURCE_OUTPUT, "\n", 1);
            Gbl_CurrentColumn++;
        }
    }

    FlSetLineNumber (LineNumber);

    /* Second argument is the optional filename (in double quotes) */

    Token = strtok (NULL, " \"");
    if (Token)
    {
        Filename = ACPI_ALLOCATE_ZEROED (strlen (Token) + 1);
        strcpy (Filename, Token);
        FlSetFilename (Filename);
    }

    /* Third argument is not supported at this time */

ResetAndExit:

    /* Reset globals for a new line */

    Gbl_CurrentLineOffset += Gbl_CurrentColumn;
    Gbl_CurrentColumn = 0;
    Gbl_LineBufPtr = Gbl_CurrentLineBuffer;
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

int
AslPopInputFileStack (
    void)
{
    ASL_FILE_NODE           *Fnode;


    Fnode = Gbl_IncludeFileStack;
    DbgPrint (ASL_PARSE_OUTPUT, "\nPop InputFile Stack, Fnode %p\n\n", Fnode);

    if (!Fnode)
    {
        return (-1);
    }

    /* Close the current include file */

    fclose (yyin);

    /* Update the top-of-stack */

    Gbl_IncludeFileStack = Fnode->Next;

    /* Reset global line counter and filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = Fnode->Filename;
    Gbl_CurrentLineNumber = Fnode->CurrentLineNumber;

    /* Point the parser to the popped file */

    yy_delete_buffer (YY_CURRENT_BUFFER);
    yy_switch_to_buffer (Fnode->State);

    /* All done with this node */

    ACPI_FREE (Fnode);
    return (0);
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
AslPushInputFileStack (
    FILE                    *InputFile,
    char                    *Filename)
{
    ASL_FILE_NODE           *Fnode;
    YY_BUFFER_STATE         State;


    /* Save the current state in an Fnode */

    Fnode = UtLocalCalloc (sizeof (ASL_FILE_NODE));

    Fnode->File                 = yyin;
    Fnode->Next                 = Gbl_IncludeFileStack;
    Fnode->State                = YY_CURRENT_BUFFER;
    Fnode->CurrentLineNumber    = Gbl_CurrentLineNumber;
    Fnode->Filename             = Gbl_Files[ASL_FILE_INPUT].Filename;

    /* Push it on the stack */

    Gbl_IncludeFileStack = Fnode;

    /* Point the parser to this file */

    State = yy_create_buffer (InputFile, YY_BUF_SIZE);
    yy_switch_to_buffer (State);

    DbgPrint (ASL_PARSE_OUTPUT, "\nPush InputFile Stack, returning %p\n\n", InputFile);

    /* Reset the global line count and filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = Filename;
    Gbl_CurrentLineNumber = 1;
    yyin = InputFile;
}


/*******************************************************************************
 *
 * FUNCTION:    AslResetCurrentLineBuffer
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Reset the Line Buffer to zero, increment global line numbers.
 *
 ******************************************************************************/

void
AslResetCurrentLineBuffer (
    void)
{

    if (Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Handle)
    {
        FlWriteFile (ASL_FILE_SOURCE_OUTPUT, Gbl_CurrentLineBuffer,
            Gbl_LineBufPtr - Gbl_CurrentLineBuffer);
    }

    Gbl_CurrentLineOffset += Gbl_CurrentColumn;
    Gbl_CurrentColumn = 0;

    Gbl_CurrentLineNumber++;
    Gbl_LogicalLineNumber++;
    Gbl_LineBufPtr = Gbl_CurrentLineBuffer;
}


/*******************************************************************************
 *
 * FUNCTION:    AslInsertLineBuffer
 *
 * PARAMETERS:  SourceChar      - One char from the input ASL source file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Put one character of the source file into the temp line buffer
 *
 ******************************************************************************/

void
AslInsertLineBuffer (
    int                     SourceChar)
{
    UINT32                  i;
    UINT32                  Count = 1;


    if (SourceChar == EOF)
    {
        return;
    }

    Gbl_InputByteCount++;

    /* Handle tabs. Convert to spaces */

    if (SourceChar == '\t')
    {
        SourceChar = ' ';
        Count = ASL_SPACES_PER_TAB -
                    (Gbl_CurrentColumn & (ASL_SPACES_PER_TAB-1));
    }

    for (i = 0; i < Count; i++)
    {
        Gbl_CurrentColumn++;

        /* Insert the character into the line buffer */

        *Gbl_LineBufPtr = (UINT8) SourceChar;
        Gbl_LineBufPtr++;

        if (Gbl_LineBufPtr > (Gbl_CurrentLineBuffer + (Gbl_LineBufferSize - 1)))
        {
#if 0
            /*
             * Warning if we have split a long source line.
             * <Probably overkill>
             */
            sprintf (MsgBuffer, "Max %u", Gbl_LineBufferSize);
            AslCommonError (ASL_WARNING, ASL_MSG_LONG_LINE,
                            Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                            Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                            Gbl_Files[ASL_FILE_INPUT].Filename, MsgBuffer);
#endif

            AslResetCurrentLineBuffer ();
        }
        else if (SourceChar == '\n')
        {
            /* End of line */

            AslResetCurrentLineBuffer ();
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    count
 *
 * PARAMETERS:  yytext      - Contains the matched keyword.
 *              Type        - Keyword/Character type:
 *                             0 = anything except a keyword
 *                             1 = pseudo-keywords
 *                             2 = non-executable ASL keywords
 *                             3 = executable ASL keywords
 *
 * RETURN:      None
 *
 * DESCRIPTION: Count keywords and put them into the line buffer
 *
 ******************************************************************************/

static void
count (
    int                 Type)
{
    int                 i;


    switch (Type)
    {
    case 2:

        TotalKeywords++;
        TotalNamedObjects++;
        break;

    case 3:

        TotalKeywords++;
        TotalExecutableOpcodes++;
        break;

    default:

        break;
    }

    for (i = 0; (yytext[i] != 0) && (yytext[i] != EOF); i++)
    {
        AslInsertLineBuffer (yytext[i]);
        *Gbl_LineBufPtr = 0;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoComment
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a standard comment.
 *
 ******************************************************************************/

static char
AslDoComment (
    void)
{
    int                 c;
    int                 c1 = 0;


    AslInsertLineBuffer ('/');
    AslInsertLineBuffer ('*');

loop:

    /* Eat chars until end-of-comment */

    while ((c = input()) != '*' && c != EOF)
    {
        AslInsertLineBuffer (c);
        c1 = c;
    }

    if (c == EOF)
    {
        goto EarlyEOF;
    }

    /*
     * Check for nested comment -- can help catch cases where a previous
     * comment was accidently left unterminated
     */
    if ((c1 == '/') && (c == '*'))
    {
        AslCommonError (ASL_WARNING, ASL_MSG_NESTED_COMMENT,
                        Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                        Gbl_InputByteCount, Gbl_CurrentColumn,
                        Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
    }

    /* Comment is closed only if the NEXT character is a slash */

    AslInsertLineBuffer (c);

    if ((c1 = input()) != '/' && c1 != EOF)
    {
        unput(c1);
        goto loop;
    }

    if (c1 == EOF)
    {
        goto EarlyEOF;
    }

    AslInsertLineBuffer (c1);
    return (TRUE);


EarlyEOF:
    /*
     * Premature End-Of-File
     */
    AslCommonError (ASL_ERROR, ASL_MSG_EARLY_EOF,
                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoCommentType2
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a new "//" comment.
 *
 ******************************************************************************/

static char
AslDoCommentType2 (
    void)
{
    int                 c;


    AslInsertLineBuffer ('/');
    AslInsertLineBuffer ('/');

    while ((c = input()) != '\n' && c != EOF)
    {
        AslInsertLineBuffer (c);
    }

    if (c == EOF)
    {
        /* End of file is OK, change to newline. Let parser detect EOF later */

        c = '\n';
    }

    AslInsertLineBuffer (c);
    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoStringLiteral
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a string literal (surrounded by quotes)
 *
 ******************************************************************************/

static char
AslDoStringLiteral (
    void)
{
    char                *StringBuffer = MsgBuffer;
    char                *EndBuffer = MsgBuffer + ASL_MSG_BUFFER_SIZE;
    char                *CleanString;
    int                 StringChar;
    UINT32              State = ASL_NORMAL_CHAR;
    UINT32              i = 0;
    UINT8               Digit;
    char                ConvertBuffer[4];


    /*
     * Eat chars until end-of-literal.
     * NOTE:  Put back the original surrounding quotes into the
     * source line buffer.
     */
    AslInsertLineBuffer ('\"');
    while ((StringChar = input()) != EOF)
    {
        AslInsertLineBuffer (StringChar);

DoCharacter:
        switch (State)
        {
        case ASL_NORMAL_CHAR:

            switch (StringChar)
            {
            case '\\':
                /*
                 * Special handling for backslash-escape sequence. We will
                 * toss the backslash and translate the escape char(s).
                 */
                State = ASL_ESCAPE_SEQUENCE;
                continue;

            case '\"':

                /* String terminator */

                goto CompletedString;

            default:

                break;
            }
            break;


        case ASL_ESCAPE_SEQUENCE:

            State = ASL_NORMAL_CHAR;
            switch (StringChar)
            {
            case 'a':

                StringChar = 0x07;      /* BELL */
                break;

            case 'b':

                StringChar = 0x08;      /* BACKSPACE */
                break;

            case 'f':

                StringChar = 0x0C;      /* FORMFEED */
                break;

            case 'n':

                StringChar = 0x0A;      /* LINEFEED */
                break;

            case 'r':

                StringChar = 0x0D;      /* CARRIAGE RETURN*/
                break;

            case 't':

                StringChar = 0x09;      /* HORIZONTAL TAB */
                break;

            case 'v':

                StringChar = 0x0B;      /* VERTICAL TAB */
                break;

            case 'x':

                State = ASL_HEX_CONSTANT;
                i = 0;
                continue;

            case '\'':                  /* Single Quote */
            case '\"':                  /* Double Quote */
            case '\\':                  /* Backslash */

                break;

            default:

                /* Check for an octal digit (0-7) */

                if (ACPI_IS_OCTAL_DIGIT (StringChar))
                {
                    State = ASL_OCTAL_CONSTANT;
                    ConvertBuffer[0] = StringChar;
                    i = 1;
                    continue;
                }

                /* Unknown escape sequence issue warning, but use the character */

                AslCommonError (ASL_WARNING, ASL_MSG_INVALID_ESCAPE,
                                Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                                Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                                Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
                break;
            }
            break;


        case ASL_OCTAL_CONSTANT:

            /* Up to three octal digits allowed */

            if (!ACPI_IS_OCTAL_DIGIT (StringChar) ||
                (i > 2))
            {
                /*
                 * Reached end of the constant. Convert the assembled ASCII
                 * string and resume processing of the next character
                 */
                ConvertBuffer[i] = 0;
                Digit = (UINT8) ACPI_STRTOUL (ConvertBuffer, NULL, 8);

                /* Check for NULL or non-ascii character (ignore if so) */

                if ((Digit == 0) || (Digit > ACPI_ASCII_MAX))
                {
                    AslCommonError (ASL_WARNING, ASL_MSG_INVALID_STRING,
                                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
                }
                else
                {
                    *StringBuffer = (char) Digit;
                    StringBuffer++;
                    if (StringBuffer >= EndBuffer)
                    {
                        goto BufferOverflow;
                    }
                }

                State = ASL_NORMAL_CHAR;
                goto DoCharacter;
                break;
            }

            /* Append another digit of the constant */

            ConvertBuffer[i] = StringChar;
            i++;
            continue;

        case ASL_HEX_CONSTANT:

            /* Up to two hex digits allowed */

            if (!ACPI_IS_XDIGIT (StringChar) ||
                (i > 1))
            {
                /*
                 * Reached end of the constant. Convert the assembled ASCII
                 * string and resume processing of the next character
                 */
                ConvertBuffer[i] = 0;
                Digit = (UINT8) ACPI_STRTOUL (ConvertBuffer, NULL, 16);

                /* Check for NULL or non-ascii character (ignore if so) */

                if ((Digit == 0) || (Digit > ACPI_ASCII_MAX))
                {
                    AslCommonError (ASL_WARNING, ASL_MSG_INVALID_STRING,
                                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
                }
                else
                {
                    *StringBuffer = (char) Digit;
                    StringBuffer++;
                    if (StringBuffer >= EndBuffer)
                    {
                        goto BufferOverflow;
                    }
                }

                State = ASL_NORMAL_CHAR;
                goto DoCharacter;
                break;
            }

            /* Append another digit of the constant */

            ConvertBuffer[i] = StringChar;
            i++;
            continue;

        default:

            break;
        }

        /* Save the finished character */

        *StringBuffer = StringChar;
        StringBuffer++;
        if (StringBuffer >= EndBuffer)
        {
            goto BufferOverflow;
        }
    }

    /*
     * Premature End-Of-File
     */
    AslCommonError (ASL_ERROR, ASL_MSG_EARLY_EOF,
                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
    return (FALSE);


CompletedString:
    /*
     * Null terminate the input string and copy string to a new buffer
     */
    *StringBuffer = 0;

    CleanString = UtGetStringBuffer (strlen (MsgBuffer) + 1);
    if (!CleanString)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_MEMORY_ALLOCATION,
                        Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                        Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                        Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
        return (FALSE);
    }

    ACPI_STRCPY (CleanString, MsgBuffer);
    AslCompilerlval.s = CleanString;
    return (TRUE);


BufferOverflow:

    /* Literal was too long */

    AslCommonError (ASL_ERROR, ASL_MSG_STRING_LENGTH,
                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                    Gbl_Files[ASL_FILE_INPUT].Filename, "Max length 4096");
    return (FALSE);
}
