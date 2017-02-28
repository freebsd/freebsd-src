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
#include "acparser.h"
#include "amlcode.h"
#include "acdebug.h"
#include "acconvert.h"


static void
CvPrintInclude(
    ACPI_FILE_NODE          *FNode,
    UINT32                  Level);

static BOOLEAN
CvListIsSingleton (
    ACPI_COMMENT_NODE       *CommentList);


/*******************************************************************************
 *
 * FUNCTION:    CvPrintOneCommentList
 *
 * PARAMETERS:  CommentList
 *              Level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints all comments within the given list.
 *              This is referred as ASL_CV_PRINT_ONE_COMMENT_LIST.
 *
 ******************************************************************************/

void
CvPrintOneCommentList (
    ACPI_COMMENT_NODE       *CommentList,
    UINT32                  Level)
{
    ACPI_COMMENT_NODE       *Current = CommentList;
    ACPI_COMMENT_NODE       *Previous;


    while (Current)
    {
        Previous = Current;
        if (Current->Comment)
        {
            AcpiDmIndent(Level);
            AcpiOsPrintf("%s\n", Current->Comment);
            Current->Comment = NULL;
        }
        Current = Current->Next;
        AcpiOsReleaseObject(AcpiGbl_RegCommentCache, Previous);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvListIsSingleton
 *
 * PARAMETERS:  CommentList -- check to see if this is a single item list.
 *
 * RETURN:      BOOLEAN
 *
 * DESCRIPTION: Returns TRUE if CommentList only contains 1 node.
 *
 ******************************************************************************/

static BOOLEAN
CvListIsSingleton (
    ACPI_COMMENT_NODE       *CommentList)

{
    if (!CommentList)
    {
        return FALSE;
    }
    else if (CommentList->Next)
    {
        return FALSE;
    }

    return TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    CvPrintOneCommentType
 *
 * PARAMETERS:  Op
 *              CommentType
 *              EndStr - String to print after printing the comment
 *              Level  - indentation level for comment lists.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints all comments of CommentType within the given Op and
 *              clears the printed comment from the Op.
 *              This is referred as ASL_CV_PRINT_ONE_COMMENT.
 *
 ******************************************************************************/

void
CvPrintOneCommentType (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   CommentType,
    char*                   EndStr,
    UINT32                  Level)
{
    BOOLEAN                 CommentExists = FALSE;
    char                    **CommentToPrint = NULL;


    switch (CommentType)
    {
    case AML_COMMENT_STANDARD:

        if (CvListIsSingleton (Op->Common.CommentList))
        {
            CvPrintOneCommentList (Op->Common.CommentList, Level);
            AcpiOsPrintf ("\n");
        }
        else
        {
            CvPrintOneCommentList (Op->Common.CommentList, Level);
        }
        Op->Common.CommentList = NULL;
        return;

    case AML_COMMENT_ENDBLK:

        if (Op->Common.EndBlkComment)
        {
            CvPrintOneCommentList (Op->Common.EndBlkComment, Level);
            Op->Common.EndBlkComment = NULL;
            AcpiDmIndent(Level);
        }
        return;

    case AMLCOMMENT_INLINE:

        CommentToPrint = &Op->Common.InlineComment;
        break;

    case AML_COMMENT_END_NODE:

        CommentToPrint = &Op->Common.EndNodeComment;
        break;

    case AML_NAMECOMMENT:

        CommentToPrint = &Op->Common.NameComment;
        break;

    case AML_COMMENT_CLOSE_BRACE:

        CommentToPrint = &Op->Common.CloseBraceComment;
        break;

    default:
        return;
    }

    if (*CommentToPrint)
    {
        AcpiOsPrintf ("%s", *CommentToPrint);
        *CommentToPrint = NULL;
    }

    if (CommentExists && EndStr)
    {
        AcpiOsPrintf ("%s", EndStr);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvCloseBraceWriteComment
 *
 * PARAMETERS:  Op
 *              Level
 *
 * RETURN:      none
 *
 * DESCRIPTION: Print a close brace } and any open brace comments associated
 *              with this parse object.
 *              This is referred as ASL_CV_CLOSE_BRACE.
 *
 ******************************************************************************/

void
CvCloseBraceWriteComment(
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level)
{
    if (!Gbl_CaptureComments)
    {
        AcpiOsPrintf ("}");
        return;
    }

    CvPrintOneCommentType (Op, AML_COMMENT_ENDBLK, NULL, Level);
    AcpiOsPrintf ("}");
    CvPrintOneCommentType (Op, AML_COMMENT_CLOSE_BRACE, NULL, Level);
}


/*******************************************************************************
 *
 * FUNCTION:    CvCloseParenWriteComment
 *
 * PARAMETERS:  Op
 *              Level
 *
 * RETURN:      none
 *
 * DESCRIPTION: Print a closing paren ) and any end node comments associated
 *              with this parse object.
 *              This is referred as ASL_CV_CLOSE_PAREN.
 *
 ******************************************************************************/

void
CvCloseParenWriteComment(
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level)
{
    if (!Gbl_CaptureComments)
    {
        AcpiOsPrintf (")");
        return;
    }

    /*
     * If this op has a BLOCK_BRACE, then output the comment when the
     * disassembler calls CvCloseBraceWriteComment
     */
    if (AcpiDmBlockType (Op) == BLOCK_PAREN)
    {
        CvPrintOneCommentType (Op, AML_COMMENT_ENDBLK, NULL, Level);
    }

    AcpiOsPrintf (")");

    if (Op->Common.EndNodeComment)
    {
        CvPrintOneCommentType (Op, AML_COMMENT_END_NODE, NULL, Level);
    }
    else if ((Op->Common.Parent->Common.AmlOpcode == AML_IF_OP) &&
         Op->Common.Parent->Common.EndNodeComment)
    {
        CvPrintOneCommentType (Op->Common.Parent,
            AML_COMMENT_END_NODE, NULL, Level);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvFileHasSwitched
 *
 * PARAMETERS:  Op
 *
 * RETURN:      BOOLEAN
 *
 * DESCRIPTION: Determine whether if a file has switched.
 *              TRUE - file has switched.
 *              FALSE - file has not switched.
 *              This is referred as ASL_CV_FILE_HAS_SWITCHED.
 *
 ******************************************************************************/

BOOLEAN
CvFileHasSwitched(
    ACPI_PARSE_OBJECT       *Op)
{
    if (Op->Common.CvFilename   &&
        AcpiGbl_CurrentFilename &&
        AcpiUtStricmp(Op->Common.CvFilename, AcpiGbl_CurrentFilename))
    {
        return TRUE;
    }
    return FALSE;
}


/*******************************************************************************
 *
 * FUNCTION:    CvPrintInclude
 *
 * PARAMETERS:  FNode - Write an Include statement for the file that is pointed
 *                      by FNode->File.
 *              Level - indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write the ASL Include statement for FNode->File in the file
 *              indicated by FNode->Parent->File. Note this function emits
 *              actual ASL code rather than comments. This switches the output
 *              file to FNode->Parent->File.
 *
 ******************************************************************************/

static void
CvPrintInclude(
    ACPI_FILE_NODE          *FNode,
    UINT32                  Level)
{
    if (!FNode || FNode->IncludeWritten)
    {
        return;
    }

    CvDbgPrint ("Writing include for %s within %s\n", FNode->Filename, FNode->Parent->Filename);
    AcpiOsRedirectOutput (FNode->Parent->File);
    CvPrintOneCommentList (FNode->IncludeComment, Level);
    AcpiDmIndent (Level);
    AcpiOsPrintf ("Include (\"%s\")\n", FNode->Filename);
    CvDbgPrint ("emitted the following: Include (\"%s\")\n", FNode->Filename);
    FNode->IncludeWritten = TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    CvSwitchFiles
 *
 * PARAMETERS:  Level - indentation level
 *              Op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Switch the outputfile and write ASL Include statement. Note,
 *              this function emits actual ASL code rather than comments.
 *              This is referred as ASL_CV_SWITCH_FILES.
 *
 ******************************************************************************/

void
CvSwitchFiles(
    UINT32                  Level,
    ACPI_PARSE_OBJECT       *Op)
{
    char                    *Filename = Op->Common.CvFilename;
    ACPI_FILE_NODE          *FNode;

    CvDbgPrint ("Switching from %s to %s\n", AcpiGbl_CurrentFilename, Filename);
    FNode = CvFilenameExists (Filename, AcpiGbl_FileTreeRoot);
    if (!FNode)
    {
        /*
         * At this point, each Filename should exist in AcpiGbl_FileTreeRoot
         * if it does not exist, then abort.
         */
        FlDeleteFile (ASL_FILE_AML_OUTPUT);
        sprintf (MsgBuffer, "\"Cannot find %s\" - %s", Filename, strerror (errno));
        AslCommonError (ASL_ERROR, ASL_MSG_OPEN, 0, 0, 0, 0, NULL, MsgBuffer);
        AslAbort ();
    }

    /*
     * If the previous file is a descendent of the current file,
     * make sure that Include statements from the current file
     * to the previous have been emitted.
     */
    while (FNode &&
           FNode->Parent &&
           AcpiUtStricmp (FNode->Filename, AcpiGbl_CurrentFilename))
    {
        CvPrintInclude (FNode, Level);
        FNode = FNode->Parent;
    }

    /* Redirect output to the Op->Common.CvFilename */

    FNode = CvFilenameExists (Filename, AcpiGbl_FileTreeRoot);
    AcpiOsRedirectOutput (FNode->File);
    AcpiGbl_CurrentFilename = FNode->Filename;
}
