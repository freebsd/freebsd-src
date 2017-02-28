/******************************************************************************
 *
 * Module Name: cvcompiler - ASL-/ASL+ converter functions
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "amlcode.h"
#include "acapps.h"
#include "acconvert.h"


/*******************************************************************************
 *
 * FUNCTION:    CvProcessComment
 *
 * PARAMETERS:  CurrentState      Current comment parse state
 *              StringBuffer      Buffer containing the comment being processed
 *              c1                Current input
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a single line comment of a c Style comment. This
 *              function captures a line of a c style comment in a char* and
 *              places the comment in the approperiate global buffer.
 *
 ******************************************************************************/

void
CvProcessComment (
    ASL_COMMENT_STATE       CurrentState,
    char                    *StringBuffer,
    int                     c1)
{
    UINT64                  i;
    char                    *LineToken;
    char                    *FinalLineToken;
    BOOLEAN                 CharStart;
    char                    *CommentString;
    char                    *FinalCommentString;


    if (Gbl_CaptureComments && CurrentState.CaptureComments)
    {
        *StringBuffer = (char) c1;
        ++StringBuffer;
        *StringBuffer = 0;
        CvDbgPrint ("Multi-line comment\n");
        CommentString = UtStringCacheCalloc (strlen (MsgBuffer) + 1);
        strcpy (CommentString, MsgBuffer);

        CvDbgPrint ("CommentString: %s\n", CommentString);

        /*
         * Determine whether if this comment spans multiple lines.
         * If so, break apart the comment by line so that it can be
         * properly indented.
         */
        if (strchr (CommentString, '\n') != NULL)
        {
            /*
             * Get the first token. The for loop pads subsequent lines
             * for comments similar to the style of this comment.
             */
            LineToken = strtok (CommentString, "\n");
            FinalLineToken = UtStringCacheCalloc (strlen (LineToken) + 1);
            strcpy (FinalLineToken, LineToken);

            /* Get rid of any carriage returns */

            if (FinalLineToken[strlen (FinalLineToken) - 1] == 0x0D)
            {
                FinalLineToken[strlen(FinalLineToken)-1] = 0;
            }
            CvAddToCommentList (FinalLineToken);
            LineToken = strtok (NULL, "\n");
            while (LineToken != NULL)
            {
                /*
                 * It is assumed that each line has some sort of indentation.
                 * This means that we need to find the first character that is not
                 * a white space within each line.
                 */
                CharStart = FALSE;
                for (i = 0; (i < (strlen (LineToken) + 1)) && !CharStart; i++)
                {
                    if (LineToken[i] != ' ' && LineToken[i] != '\t')
                    {
                        CharStart = TRUE;
                        LineToken += i-1;
                        LineToken [0] = ' '; /* Pad for Formatting */
                    }
                }
                FinalLineToken = UtStringCacheCalloc (strlen (LineToken) + 1);
                strcat (FinalLineToken, LineToken);

                /* Get rid of any carriage returns */

                if (FinalLineToken[strlen (FinalLineToken) - 1] == 0x0D)
                {
                    FinalLineToken[strlen(FinalLineToken) - 1] = 0;
                }
                CvAddToCommentList (FinalLineToken);
                LineToken = strtok (NULL,"\n");
            }
        }

        /*
         * If this only spans a single line, check to see whether if this comment
         * appears on the same line as a line of code. If does, retain it's
         * position for stylistic reasons. If it doesn't, add it to the comment
         * List so that it can be associated with the next node that's created.
         */
        else
        {
           /*
            * if this is not a regular comment, pad with extra spaces that appeared
            * in the original source input to retain the original spacing.
            */
            FinalCommentString = UtStringCacheCalloc (strlen (CommentString) + CurrentState.SpacesBefore + 1);
            for (i=0; (CurrentState.CommentType != ASL_COMMENT_STANDARD) &&
                (i < CurrentState.SpacesBefore); ++i)
            {
                 FinalCommentString[i] = ' ';
            }
            strcat (FinalCommentString, CommentString);
            CvPlaceComment (CurrentState.CommentType, FinalCommentString);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvProcessCommentType2
 *
 * PARAMETERS:  CurrentState      Current comment parse state
 *              StringBuffer      Buffer containing the comment being processed
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a single line comment. This function captures a comment
 *              in a char* and places the comment in the approperiate global
 *              buffer through CvPlaceComment
 *
 ******************************************************************************/

void
CvProcessCommentType2 (
    ASL_COMMENT_STATE       CurrentState,
    char                    *StringBuffer)
{
    UINT32                  i;
    char                    *CommentString;
    char                    *FinalCommentString;


    if (Gbl_CaptureComments && CurrentState.CaptureComments)
    {
        *StringBuffer = 0; /* null terminate */
        CvDbgPrint ("Single-line comment\n");
        CommentString = UtStringCacheCalloc (strlen (MsgBuffer) + 1);
        strcpy (CommentString, MsgBuffer);

        /* If this comment lies on the same line as the latest parse node,
         * assign it to that node's CommentAfter field. Saving in this field
         * will allow us to support comments that come after code on the same
         * line as the code itself. For example,
         * Name(A,"") //comment
         *
         * will be retained rather than transformed into
         *
         * Name(A,"")
         * //comment
         *
         * For this case, we only need to add one comment since
         *
         * Name(A,"") //comment1 //comment2 ... more comments here.
         *
         * would be lexically analyzed as a single comment.
         *
         * Create a new string with the approperiate spaces. Since we need
         * to account for the proper spacing, the actual comment,
         * extra 2 spaces so that this comment can be converted to the "/ *"
         * style and the null terminator, the string would look something like
         *
         * [ (spaces) (comment)  ( * /) ('\0') ]
         *
         */
        FinalCommentString = UtStringCacheCalloc (CurrentState.SpacesBefore + strlen (CommentString) + 3 + 1);
        for (i=0; (CurrentState.CommentType!=1) && (i<CurrentState.SpacesBefore); ++i)
        {
            FinalCommentString[i] = ' ';
        }
        strcat (FinalCommentString, CommentString);

        /* convert to a "/ *" style comment  */

        strcat (FinalCommentString, " */");
        FinalCommentString [CurrentState.SpacesBefore + strlen (CommentString) + 3] = 0;

        /* get rid of the carriage return */

        if (FinalCommentString[strlen (FinalCommentString) - 1] == 0x0D)
        {
            FinalCommentString[strlen(FinalCommentString)-1] = 0;
        }
        CvPlaceComment (CurrentState.CommentType, FinalCommentString);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CgCalculateCommentLengths
 *
 * PARAMETERS:  Op                 - Calculate all comments of this Op
 *
 * RETURN:      TotalCommentLength - Length of all comments within this node.
 *
 * DESCRIPTION: calculate the length that the each comment takes up within Op.
 *              Comments look like the follwoing: [0xA9 OptionBtye comment 0x00]
 *              therefore, we add 1 + 1 + strlen (comment) + 1 to get the actual
 *              length of this comment.
 *
 ******************************************************************************/

UINT32
CvCalculateCommentLengths(
   ACPI_PARSE_OBJECT        *Op)
{
    UINT32                  CommentLength = 0;
    UINT32                  TotalCommentLength = 0;
    ACPI_COMMENT_NODE       *Current = NULL;


    if (!Gbl_CaptureComments)
    {
        return (0);
    }

    CvDbgPrint ("==Calculating comment lengths for %s\n",  Op->Asl.ParseOpName);
    if (Op->Asl.FileChanged)
    {
        TotalCommentLength += strlen (Op->Asl.Filename) + 3;

        if (Op->Asl.ParentFilename &&
            AcpiUtStricmp (Op->Asl.Filename, Op->Asl.ParentFilename))
        {
            TotalCommentLength += strlen (Op->Asl.ParentFilename) + 3;
        }
    }
    if (Op->Asl.CommentList)
    {
        Current = Op->Asl.CommentList;
        while (Current)
        {
            CommentLength = strlen (Current->Comment)+3;
            CvDbgPrint ("Length of standard comment: %d\n", CommentLength);
            CvDbgPrint ("    Comment string: %s\n\n", Current->Comment);
            TotalCommentLength += CommentLength;
            Current = Current->Next;
        }
    }
    if (Op->Asl.EndBlkComment)
    {
        Current = Op->Asl.EndBlkComment;
        while (Current)
        {
            CommentLength = strlen (Current->Comment)+3;
            CvDbgPrint ("Length of endblkcomment: %d\n", CommentLength);
            CvDbgPrint ("    Comment string: %s\n\n", Current->Comment);
            TotalCommentLength += CommentLength;
            Current = Current->Next;
        }
    }
    if (Op->Asl.InlineComment)
    {
        CommentLength = strlen (Op->Asl.InlineComment)+3;
        CvDbgPrint ("Length of inline comment: %d\n", CommentLength);
        CvDbgPrint ("    Comment string: %s\n\n", Op->Asl.InlineComment);
        TotalCommentLength += CommentLength;
    }
    if (Op->Asl.EndNodeComment)
    {
        CommentLength = strlen(Op->Asl.EndNodeComment)+3;
        CvDbgPrint ("Length of end node comment +3: %d\n", CommentLength);
        CvDbgPrint ("    Comment string: %s\n\n", Op->Asl.EndNodeComment);
        TotalCommentLength += CommentLength;
    }

    if (Op->Asl.CloseBraceComment)
    {
        CommentLength = strlen (Op->Asl.CloseBraceComment)+3;
        CvDbgPrint ("Length of close brace comment: %d\n", CommentLength);
        CvDbgPrint ("    Comment string: %s\n\n", Op->Asl.CloseBraceComment);
        TotalCommentLength += CommentLength;
    }

    CvDbgPrint("\n\n");

    return TotalCommentLength;

}


/*******************************************************************************
 *
 * FUNCTION:    CgWriteAmlDefBlockComment
 *
 * PARAMETERS:  Op              - Current parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write all comments for a particular definition block.
 *              For definition blocks, the comments need to come after the
 *              definition block header. The regular comments above the
 *              definition block would be categorized as
 *              STD_DEFBLK_COMMENT and comments after the closing brace
 *              is categorized as END_DEFBLK_COMMENT.
 *
 ******************************************************************************/

void
CgWriteAmlDefBlockComment(
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   CommentOption;
    ACPI_COMMENT_NODE       *Current;
    char                    *NewFilename;
    char                    *Position;
    char                    *DirectoryPosition;


    if (!Gbl_CaptureComments ||
        (Op->Asl.ParseOpcode != PARSEOP_DEFINITION_BLOCK))
    {
        return;
    }

    CvDbgPrint ("Printing comments for a definition block..\n");

    /* first, print the file name comment after changing .asl to .dsl */

    NewFilename = UtStringCacheCalloc (strlen (Op->Asl.Filename));
    strcpy (NewFilename, Op->Asl.Filename);
    DirectoryPosition = strrchr (NewFilename, '/');
    Position = strrchr (NewFilename, '.');

    if (Position && (Position > DirectoryPosition))
    {
        /* Tack on the new suffix */

        Position++;
        *Position = 0;
        strcat (Position, FILE_SUFFIX_DISASSEMBLY);
    }
    else
    {
        /* No dot, add one and then the suffix */

        strcat (NewFilename, ".");
        strcat (NewFilename, FILE_SUFFIX_DISASSEMBLY);
    }

    CommentOption = FILENAME_COMMENT;
    CgWriteOneAmlComment(Op, NewFilename, CommentOption);

    Current = Op->Asl.CommentList;
    CommentOption = STD_DEFBLK_COMMENT;
    while (Current)
    {
        CgWriteOneAmlComment(Op, Current->Comment, CommentOption);
        CvDbgPrint ("Printing comment: %s\n", Current->Comment);
        Current = Current->Next;
    }
    Op->Asl.CommentList = NULL;

    /* print any Inline comments associated with this node */

    if (Op->Asl.CloseBraceComment)
    {
        CommentOption = END_DEFBLK_COMMENT;
        CgWriteOneAmlComment(Op, Op->Asl.CloseBraceComment, CommentOption);
        Op->Asl.CloseBraceComment = NULL;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CgWriteOneAmlComment
 *
 * PARAMETERS:  Op              - Current parse op
 *              CommentToPrint  - Comment that's printed
 *              InputOption     - Denotes the comment option.
 *
 * RETURN:      None
 *
 * DESCRIPTION: write a single comment.
 *
 ******************************************************************************/

void
CgWriteOneAmlComment(
    ACPI_PARSE_OBJECT       *Op,
    char*                   CommentToPrint,
    UINT8                   InputOption)
{
    UINT8 CommentOption = InputOption;
    UINT8 CommentOpcode = (UINT8)AML_COMMENT_OP;

    CgLocalWriteAmlData (Op, &CommentOpcode, 1);
    CgLocalWriteAmlData (Op, &CommentOption, 1);

    /* The strlen (..) + 1 is to include the null terminator */

    CgLocalWriteAmlData (Op, CommentToPrint, strlen (CommentToPrint) + 1);
}


/*******************************************************************************
 *
 * FUNCTION:    CgWriteAmlComment
 *
 * PARAMETERS:  Op              - Current parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: write all comments pertaining to the
 *              current parse op
 *
 ******************************************************************************/

void
CgWriteAmlComment(
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_COMMENT_NODE       *Current;
    UINT8                   CommentOption;
    char                    *NewFilename;
    char                    *ParentFilename;


    if ((Op->Asl.ParseOpcode == PARSEOP_DEFINITION_BLOCK) ||
         !Gbl_CaptureComments)
    {
        return;
    }

    /* Print out the filename comment if needed */

    if (Op->Asl.FileChanged)
    {

        /* first, print the file name comment after changing .asl to .dsl */

        NewFilename =
            FlGenerateFilename (Op->Asl.Filename, FILE_SUFFIX_DISASSEMBLY);
        CvDbgPrint ("Writing file comment, \"%s\" for %s\n",
            NewFilename, Op->Asl.ParseOpName);
        CgWriteOneAmlComment(Op, NewFilename, FILENAME_COMMENT);

        if (Op->Asl.ParentFilename &&
            AcpiUtStricmp (Op->Asl.ParentFilename, Op->Asl.Filename))
        {
            ParentFilename = FlGenerateFilename (Op->Asl.ParentFilename,
                FILE_SUFFIX_DISASSEMBLY);
            CgWriteOneAmlComment(Op, ParentFilename, PARENTFILENAME_COMMENT);
        }

        /* prevent multiple writes of the same comment */

        Op->Asl.FileChanged = FALSE;
    }

    /*
     * Regular comments are stored in a list of comments within an Op.
     * If there is a such list in this node, print out the comment
     * as byte code.
     */
    Current = Op->Asl.CommentList;
    if (Op->Asl.ParseOpcode == PARSEOP_INCLUDE)
    {
        CommentOption = INCLUDE_COMMENT;
    }
    else
    {
        CommentOption = STANDARD_COMMENT;
    }

    while (Current)
    {
        CgWriteOneAmlComment(Op, Current->Comment, CommentOption);
        Current = Current->Next;
    }
    Op->Asl.CommentList = NULL;

    Current = Op->Asl.EndBlkComment;
    CommentOption = ENDBLK_COMMENT;
    while (Current)
    {
        CgWriteOneAmlComment(Op, Current->Comment, CommentOption);
        Current = Current->Next;
    }
    Op->Asl.EndBlkComment = NULL;

    /* print any Inline comments associated with this node */

    if (Op->Asl.InlineComment)
    {
        CommentOption = INLINE_COMMENT;
        CgWriteOneAmlComment(Op, Op->Asl.InlineComment, CommentOption);
        Op->Asl.InlineComment = NULL;
    }

    if (Op->Asl.EndNodeComment)
    {
        CommentOption = ENDNODE_COMMENT;
        CgWriteOneAmlComment(Op, Op->Asl.EndNodeComment, CommentOption);
        Op->Asl.EndNodeComment = NULL;
    }

    if (Op->Asl.CloseBraceComment)
    {
        CommentOption = CLOSE_BRACE_COMMENT;
        CgWriteOneAmlComment(Op, Op->Asl.CloseBraceComment, CommentOption);
        Op->Asl.CloseBraceComment = NULL;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvCommentNodeCalloc
 *
 * PARAMETERS:  none
 *
 * RETURN:      Pointer to the comment node. Aborts on allocation failure
 *
 * DESCRIPTION: Allocate a string node buffer.
 *
 ******************************************************************************/

ACPI_COMMENT_NODE*
CvCommentNodeCalloc (
    void)
{
   ACPI_COMMENT_NODE        *NewCommentNode;


   NewCommentNode =
       (ACPI_COMMENT_NODE*) UtLocalCalloc (sizeof(ACPI_COMMENT_NODE));
   NewCommentNode->Next = NULL;
   return NewCommentNode;
}


/*******************************************************************************
 *
 * FUNCTION:    CvParseOpBlockType
 *
 * PARAMETERS:  Op              - Object to be examined
 *
 * RETURN:      BlockType - not a block, parens, braces, or even both.
 *
 * DESCRIPTION: Type of block for this ASL parseop (parens or braces)
 *              keep this in sync with aslprimaries.y, aslresources.y and
 *              aslrules.y
 *
 ******************************************************************************/

UINT32
CvParseOpBlockType (
    ACPI_PARSE_OBJECT       *Op)
{
    if (!Op)
    {
        return (BLOCK_NONE);
    }

    switch (Op->Asl.ParseOpcode)
    {

    /* from aslprimaries.y */

    case PARSEOP_VAR_PACKAGE:
    case PARSEOP_BANKFIELD:
    case PARSEOP_BUFFER:
    case PARSEOP_CASE:
    case PARSEOP_DEVICE:
    case PARSEOP_FIELD:
    case PARSEOP_FOR:
    case PARSEOP_FUNCTION:
    case PARSEOP_IF:
    case PARSEOP_ELSEIF:
    case PARSEOP_INDEXFIELD:
    case PARSEOP_METHOD:
    case PARSEOP_POWERRESOURCE:
    case PARSEOP_PROCESSOR:
    case PARSEOP_DATABUFFER:
    case PARSEOP_SCOPE:
    case PARSEOP_SWITCH:
    case PARSEOP_THERMALZONE:
    case PARSEOP_WHILE:

    /* from aslresources.y */

    case PARSEOP_RESOURCETEMPLATE: /* optional parens */
    case PARSEOP_VENDORLONG:
    case PARSEOP_VENDORSHORT:
    case PARSEOP_INTERRUPT:
    case PARSEOP_IRQNOFLAGS:
    case PARSEOP_IRQ:
    case PARSEOP_GPIO_INT:
    case PARSEOP_GPIO_IO:
    case PARSEOP_DMA:

    /*from aslrules.y */

    case PARSEOP_DEFINITION_BLOCK:
        return (BLOCK_PAREN | BLOCK_BRACE);

    default:

        return (BLOCK_NONE);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvProcessCommentState
 *
 * PARAMETERS:  char
 *
 * RETURN:      None
 *
 * DESCRIPTION: Take the given input. If this character is
 *              defined as a comment table entry, then update the state
 *              accordingly.
 *
 ******************************************************************************/

void
CvProcessCommentState (
    char                    input)
{

    if (input != ' ')
    {
        Gbl_CommentState.SpacesBefore = 0;
    }

    switch (input)
    {
    case '\n':

        Gbl_CommentState.CommentType = ASL_COMMENT_STANDARD;
        break;

    case ' ':

        /* Keep the CommentType the same */

        Gbl_CommentState.SpacesBefore++;
        break;

    case '(':

        Gbl_CommentState.CommentType = ASL_COMMENT_OPEN_PAREN;
        break;

    case ')':

        Gbl_CommentState.CommentType = ASL_COMMENT_CLOSE_PAREN;
        break;

    case '{':

        Gbl_CommentState.CommentType = ASL_COMMENT_STANDARD;
        Gbl_CommentState.ParsingParenBraceNode = NULL;
        CvDbgPrint ("End Parsing paren/Brace node!\n");
        break;

    case '}':

        Gbl_CommentState.CommentType = ASL_COMMENT_CLOSE_BRACE;
        break;

    case ',':

        Gbl_CommentState.CommentType = ASLCOMMENT_INLINE;
        break;

    default:

        Gbl_CommentState.CommentType = ASLCOMMENT_INLINE;
        break;

    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvAddToCommentList
 *
 * PARAMETERS:  toAdd              - Contains the comment to be inserted
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add the given char* to a list of comments in the global list
 *              of comments.
 *
 ******************************************************************************/

void
CvAddToCommentList (
    char*                   ToAdd)
{
   if (Gbl_Comment_List_Head)
   {
       Gbl_Comment_List_Tail->Next = CvCommentNodeCalloc ();
       Gbl_Comment_List_Tail = Gbl_Comment_List_Tail->Next;
   }
   else
   {
       Gbl_Comment_List_Head = CvCommentNodeCalloc ();
       Gbl_Comment_List_Tail = Gbl_Comment_List_Head;
   }

   Gbl_Comment_List_Tail->Comment = ToAdd;

   return;
}

/*******************************************************************************
 *
 * FUNCTION:    CvAppendInlineComment
 *
 * PARAMETERS:  InlineComment      - Append to the end of this string.
 *              toAdd              - Contains the comment to be inserted
 *
 * RETURN:      Str                - toAdd appended to InlineComment
 *
 * DESCRIPTION: Concatenate ToAdd to InlineComment
 *
 ******************************************************************************/

char*
CvAppendInlineComment (
    char                    *InlineComment,
    char                    *ToAdd)
{
    char*                   Str;
    UINT32                  Size = 0;


    if (!InlineComment)
    {
        return ToAdd;
    }
    if (ToAdd)
    {
        Size = strlen (ToAdd);
    }
    Size += strlen (InlineComment);
    Str = UtStringCacheCalloc (Size+1);
    strcpy (Str, InlineComment);
    strcat (Str, ToAdd);
    Str[Size+1] = 0;

    return Str;
}


/*******************************************************************************
 *
 * FUNCTION:    CvPlaceComment
 *
 * PARAMETERS:  Int           - Type
 *              char*         - CommentString
 *
 * RETURN:      None
 *
 * DESCRIPTION: Given type and CommentString, this function places the
 *              CommentString in the approperiate global comment list or char*
 *
 ******************************************************************************/

void
CvPlaceComment(
    UINT8                   Type,
    char                    *CommentString)
{
    ACPI_PARSE_OBJECT       *LatestParseNode;
    ACPI_PARSE_OBJECT       *ParenBraceNode;


    LatestParseNode = Gbl_CommentState.Latest_Parse_Node;
    ParenBraceNode  = Gbl_CommentState.ParsingParenBraceNode;
    CvDbgPrint ("Placing comment %s for type %d\n", CommentString, Type);

    switch (Type)
    {
    case ASL_COMMENT_STANDARD:

        CvAddToCommentList (CommentString);
        break;

    case ASLCOMMENT_INLINE:

        LatestParseNode->Asl.InlineComment =
            CvAppendInlineComment (LatestParseNode->Asl.InlineComment,
            CommentString);
        break;

    case ASL_COMMENT_OPEN_PAREN:

        Gbl_Inline_Comment_Buffer =
            CvAppendInlineComment(Gbl_Inline_Comment_Buffer,
            CommentString);
        break;

    case ASL_COMMENT_CLOSE_PAREN:

        if (ParenBraceNode)
        {
            ParenBraceNode->Asl.EndNodeComment =
                CvAppendInlineComment (ParenBraceNode->Asl.EndNodeComment,
                CommentString);
        }
        else
        {
            LatestParseNode->Asl.EndNodeComment =
                CvAppendInlineComment (LatestParseNode->Asl.EndNodeComment,
                CommentString);
        }
        break;

    case ASL_COMMENT_CLOSE_BRACE:

        LatestParseNode->Asl.CloseBraceComment = CommentString;
        break;

    default:

        break;

    }
}
