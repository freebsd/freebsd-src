/******************************************************************************
 *
 * Module Name: cvparser - Converter functions that are called from the AML
 *                         parser.
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
#include "acdispat.h"
#include "amlcode.h"
#include "acinterp.h"
#include "acdisasm.h"
#include "acconvert.h"


/* local prototypes */

static BOOLEAN
CvCommentExists (
    UINT8                   *Address);

static BOOLEAN
CvIsFilename (
    char                   *Filename);

static ACPI_FILE_NODE*
CvFileAddressLookup(
    char                    *Address,
    ACPI_FILE_NODE          *Head);

static void
CvAddToFileTree (
    char                    *Filename,
    char                    *PreviousFilename);

static void
CvSetFileParent (
    char                    *ChildFile,
    char                    *ParentFile);


/*******************************************************************************
 *
 * FUNCTION:    CvIsFilename
 *
 * PARAMETERS:  filename - input filename
 *
 * RETURN:      BOOLEAN - TRUE if all characters are between 0x20 and 0x7f
 *
 * DESCRIPTION: Take a given char * and see if it contains all printable
 *              characters. If all characters have hexvalues 20-7f and ends with
 *              .dsl, we will assume that it is a proper filename.
 *
 ******************************************************************************/

static BOOLEAN
CvIsFilename (
    char                    *Filename)
{
    UINT64                  Length = strlen(Filename);
    UINT64                  i;
    char                    *FileExt = Filename + Length - 4;


    if ((Length > 4) && AcpiUtStricmp (FileExt, ".dsl"))
    {
        return FALSE;
    }

    for(i = 0; i<Length; ++i)
    {
        if (!isprint (Filename[i]))
        {
            return FALSE;
        }
    }
    return TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    CvInitFileTree
 *
 * PARAMETERS:  Table      - input table
 *              AmlStart   - Address of the starting point of the AML.
 *              AmlLength  - Length of the AML file.
 *
 * RETURN:      none
 *
 * DESCRIPTION: Initialize the file dependency tree by scanning the AML.
 *              This is referred as ASL_CV_INIT_FILETREE.
 *
 ******************************************************************************/

void
CvInitFileTree (
    ACPI_TABLE_HEADER       *Table,
    UINT8                   *AmlStart,
    UINT32                  AmlLength)
{
    UINT8                   *TreeAml;
    UINT8                   *FileEnd;
    char                    *Filename = NULL;
    char                    *PreviousFilename = NULL;
    char                    *ParentFilename = NULL;
    char                    *ChildFilename = NULL;


    if (!Gbl_CaptureComments)
    {
        return;
    }

    CvDbgPrint ("AmlLength: %x\n", AmlLength);
    CvDbgPrint ("AmlStart:  %p\n", AmlStart);
    CvDbgPrint ("AmlEnd?:   %p\n", AmlStart+AmlLength);

    AcpiGbl_FileTreeRoot = AcpiOsAcquireObject (AcpiGbl_FileCache);
    AcpiGbl_FileTreeRoot->FileStart = (char *)(AmlStart);
    AcpiGbl_FileTreeRoot->FileEnd = (char *)(AmlStart + Table->Length);
    AcpiGbl_FileTreeRoot->Next = NULL;
    AcpiGbl_FileTreeRoot->Parent = NULL;
    AcpiGbl_FileTreeRoot->Filename = (char *)(AmlStart+2);

    /* Set the root file to the current open file */

    AcpiGbl_FileTreeRoot->File = AcpiGbl_OutputFile;

    /*
     * Set this to true because we dont need to output
     * an include statement for the topmost file
     */
    AcpiGbl_FileTreeRoot->IncludeWritten = TRUE;
    Filename = NULL;
    AcpiGbl_CurrentFilename = (char *)(AmlStart+2);
    AcpiGbl_RootFilename    = (char *)(AmlStart+2);

    TreeAml = AmlStart;
    FileEnd = AmlStart + AmlLength;

    while (TreeAml <= FileEnd)
    {
        /*
         * Make sure that this filename contains all printable characters
         * and a .dsl extension at the end. If not, then it must be some
         * raw data that doesn't outline a filename.
         */
        if ((*TreeAml == AML_COMMENT_OP) &&
            (*(TreeAml+1) == FILENAME_COMMENT) &&
            (CvIsFilename ((char *)(TreeAml+2))))
        {
            CvDbgPrint ("A9 and a 08 file\n");
            PreviousFilename = Filename;
            Filename = (char *) (TreeAml+2);
            CvAddToFileTree (Filename, PreviousFilename);
            ChildFilename = Filename;
            CvDbgPrint ("%s\n", Filename);
        }
        else if ((*TreeAml == AML_COMMENT_OP) &&
            (*(TreeAml+1) == PARENTFILENAME_COMMENT) &&
            (CvIsFilename ((char *)(TreeAml+2))))
        {
            CvDbgPrint ("A9 and a 09 file\n");
            ParentFilename = (char *)(TreeAml+2);
            CvSetFileParent (ChildFilename, ParentFilename);
            CvDbgPrint ("%s\n", ParentFilename);
        }
        ++TreeAml;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvClearOpComments
 *
 * PARAMETERS:  Op -- clear all comments within this Op
 *
 * RETURN:      none
 *
 * DESCRIPTION: Clear all converter-related fields of the given Op.
 *              This is referred as ASL_CV_CLEAR_OP_COMMENTS.
 *
 ******************************************************************************/

void
CvClearOpComments (
    ACPI_PARSE_OBJECT       *Op)
{
    Op->Common.InlineComment     = NULL;
    Op->Common.EndNodeComment    = NULL;
    Op->Common.NameComment       = NULL;
    Op->Common.CommentList       = NULL;
    Op->Common.EndBlkComment     = NULL;
    Op->Common.CloseBraceComment = NULL;
    Op->Common.CvFilename        = NULL;
    Op->Common.CvParentFilename  = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    CvCommentExists
 *
 * PARAMETERS:  address - check if this address appears in the list
 *
 * RETURN:      BOOLEAN - TRUE if the address exists.
 *
 * DESCRIPTION: look at the pointer address and check if this appears in the
 *              list of all addresses. If it exitsts in the list, return TRUE
 *              if it exists. Otherwise add to the list and return FALSE.
 *
 ******************************************************************************/

static BOOLEAN
CvCommentExists (
    UINT8                    *Address)
{
    ACPI_COMMENT_ADDR_NODE   *Current = AcpiGbl_CommentAddrListHead;
    UINT8                    Option;


    if (!Address)
    {
        return (FALSE);
    }
    Option = *(Address + 1);

    /*
     * FILENAME_COMMENT and PARENTFILENAME_COMMENT are not treated as comments.
     * They serve as markers for where the file starts and ends.
     */
    if ((Option == FILENAME_COMMENT) || (Option == PARENTFILENAME_COMMENT))
    {
       return (FALSE);
    }

    if (!Current)
    {
        AcpiGbl_CommentAddrListHead =
            AcpiOsAcquireObject (AcpiGbl_RegCommentCache);
        AcpiGbl_CommentAddrListHead->Addr = Address;
        AcpiGbl_CommentAddrListHead->Next = NULL;
        return (FALSE);
    }
    else
    {
        while (Current)
        {
            if (Current->Addr != Address)
            {
                Current = Current->Next;
            }
            else
            {
                return (TRUE);
            }
        }

        /*
         * If the execution gets to this point, it means that this address
         * does not exists in the list. Add this address to the
         * beginning of the list.
         */
        Current = AcpiGbl_CommentAddrListHead;
        AcpiGbl_CommentAddrListHead =
            AcpiOsAcquireObject (AcpiGbl_RegCommentCache);
        AcpiGbl_CommentAddrListHead->Addr = Address;
        AcpiGbl_CommentAddrListHead->Next = Current;
        return (FALSE);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvFilenameExists
 *
 * PARAMETERS:  Filename        - filename to search
 *
 * RETURN:      ACPI_FILE_NODE - a pointer to a file node
 *
 * DESCRIPTION: Look for the given filename in the file dependency tree.
 *              Returns the file node if it exists, returns NULL if it does not.
 *
 ******************************************************************************/

ACPI_FILE_NODE*
CvFilenameExists(
    char                    *Filename,
    ACPI_FILE_NODE          *Head)
{
    ACPI_FILE_NODE          *Current = Head;


    while (Current)
    {
        if (!AcpiUtStricmp (Current->Filename, Filename))
        {
            return (Current);
        }
        Current = Current->Next;
    }
    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    CvFileAddressLookup
 *
 * PARAMETERS:  Address        - address to look up
 *              Head           - file dependency tree
 *
 * RETURN:      ACPI_FLE_NODE - pointer to a file node containing the address
 *
 * DESCRIPTION: Look for the given address in the file dependency tree.
 *              Returns the first file node where the given address is within
 *              the file node's starting and ending address.
 *
 ******************************************************************************/

static ACPI_FILE_NODE*
CvFileAddressLookup(
    char                    *Address,
    ACPI_FILE_NODE          *Head)
{
    ACPI_FILE_NODE          *Current = Head;


    while (Current)
    {
        if ((Address >= Current->FileStart) &&
            (Address < Current->FileEnd ||
            !Current->FileEnd))
        {
            return (Current);
        }
        Current = Current->Next;
    }

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    CvLabelFileNode
 *
 * PARAMETERS:  Op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Takes a given parse op, looks up its Op->Common.Aml field
 *              within the file tree and fills in approperiate file information
 *              from a matching node within the tree.
 *              This is referred as ASL_CV_LABEL_FILENODE.
 *
 ******************************************************************************/

void
CvLabelFileNode(
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_FILE_NODE          *Node;


    if (!Op)
    {
        return;
    }

    Node = CvFileAddressLookup ((char *)Op->Common.Aml, AcpiGbl_FileTreeRoot);
    if (!Node)
    {
       return;
    }

    Op->Common.CvFilename = Node->Filename;
    if (Node->Parent)
    {
        Op->Common.CvParentFilename = Node->Parent->Filename;
    }
    else
    {
        Op->Common.CvParentFilename = Node->Filename;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvAddToFileTree
 *
 * PARAMETERS:  Filename          - Address containing the name of the current
 *                                  filename
 *              PreviousFilename  - Address containing the name of the previous
 *                                  filename
 *
 * RETURN:      void
 *
 * DESCRIPTION: Add this filename to the AcpiGbl_FileTree if it does not exist.
 *
 ******************************************************************************/

static void
CvAddToFileTree (
    char                    *Filename,
    char                    *PreviousFilename)
{
    ACPI_FILE_NODE          *Node;


    if (!AcpiUtStricmp(Filename, AcpiGbl_RootFilename) &&
        PreviousFilename)
    {
        Node = CvFilenameExists (PreviousFilename, AcpiGbl_FileTreeRoot);
        if (Node)
        {
            /*
             * Set the end point of the PreviousFilename to the address
             * of Filename.
             */
            Node->FileEnd = Filename;
        }
    }
    else if (!AcpiUtStricmp(Filename, AcpiGbl_RootFilename) &&
             !PreviousFilename)
    {
        return;
    }

    Node = CvFilenameExists (Filename, AcpiGbl_FileTreeRoot);
    if (Node && PreviousFilename)
    {
        /*
         * Update the end of the previous file and all of their parents' ending
         * Addresses. This is done to ensure that parent file ranges extend to
         * the end of their childrens' files.
         */
        Node = CvFilenameExists (PreviousFilename, AcpiGbl_FileTreeRoot);
        if (Node && (Node->FileEnd < Filename))
        {
            Node->FileEnd = Filename;
            Node = Node->Parent;
            while (Node)
            {
                if (Node->FileEnd < Filename)
                {
                    Node->FileEnd = Filename;
                }
                Node = Node->Parent;
            }
        }
    }
    else
    {
        Node = AcpiGbl_FileTreeRoot;
        AcpiGbl_FileTreeRoot = AcpiOsAcquireObject (AcpiGbl_FileCache);
        AcpiGbl_FileTreeRoot->Next = Node;
        AcpiGbl_FileTreeRoot->Parent = NULL;
        AcpiGbl_FileTreeRoot->Filename = Filename;
        AcpiGbl_FileTreeRoot->FileStart = Filename;
        AcpiGbl_FileTreeRoot->IncludeWritten = FALSE;
        AcpiGbl_FileTreeRoot->File = fopen(Filename, "w+");

        /*
         * If we can't open the file, we need to abort here before we
         * accidentally write to a NULL file.
         */
        if (!AcpiGbl_FileTreeRoot->File)
        {
            /* delete the .xxx file */

            FlDeleteFile (ASL_FILE_AML_OUTPUT);
            sprintf (MsgBuffer, "\"%s\" - %s", Filename, strerror (errno));
            AslCommonError (ASL_ERROR, ASL_MSG_OPEN, 0, 0, 0, 0, NULL, MsgBuffer);
            AslAbort ();
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvSetFileParent
 *
 * PARAMETERS:  ChildFile  - contains the filename of the child file
 *              ParentFile - contains the filename of the parent file.
 *
 * RETURN:      none
 *
 * DESCRIPTION: point the parent pointer of the Child to the node that
 *              corresponds with the parent file node.
 *
 ******************************************************************************/

static void
CvSetFileParent (
    char                    *ChildFile,
    char                    *ParentFile)
{
    ACPI_FILE_NODE          *Child;
    ACPI_FILE_NODE          *Parent;


    Child  = CvFilenameExists (ChildFile, AcpiGbl_FileTreeRoot);
    Parent = CvFilenameExists (ParentFile, AcpiGbl_FileTreeRoot);
    if (Child && Parent)
    {
        Child->Parent = Parent;

        while (Child->Parent)
        {
            if (Child->Parent->FileEnd < Child->FileStart)
            {
                Child->Parent->FileEnd = Child->FileStart;
            }
            Child = Child->Parent;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvCaptureCommentsOnly
 *
 * PARAMETERS:  ParserState         - A parser state object
 *
 * RETURN:      none
 *
 * DESCRIPTION: look at the aml that the parser state is pointing to,
 *              capture any AML_COMMENT_OP and it's arguments and increment the
 *              aml pointer past the comment. Comments are transferred to parse
 *              nodes through CvTransferComments() as well as
 *              AcpiPsBuildNamedOp().
 *              This is referred as ASL_CV_CAPTURE_COMMENTS_ONLY.
 *
 ******************************************************************************/

void
CvCaptureCommentsOnly (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT8                   *Aml = ParserState->Aml;
    UINT16                  Opcode = (UINT16) ACPI_GET8 (Aml);
    UINT32                  Length = 0;
    UINT8                   CommentOption = (UINT16) ACPI_GET8 (Aml+1);
    BOOLEAN                 StdDefBlockFlag = FALSE;
    ACPI_COMMENT_NODE       *CommentNode;
    ACPI_FILE_NODE          *FileNode;


    if (!Gbl_CaptureComments ||
        Opcode != AML_COMMENT_OP)
    {
       return;
    }

    while (Opcode == AML_COMMENT_OP)
    {
        CvDbgPrint ("comment aml address: %p\n", Aml);

        if (CvCommentExists(ParserState->Aml))
        {
            CvDbgPrint ("Avoiding capturing an existing comment.\n");
        }
        else
        {
            CommentOption = *(Aml+1);

            /* Increment past the comment option and point the approperiate char pointers.*/

            Aml += 2;

            /* found a comment. Now, set pointers to these comments. */

            switch (CommentOption)
            {
                case STD_DEFBLK_COMMENT:

                    StdDefBlockFlag = TRUE;

                    /* add to a linked list of nodes. This list will be taken by the parse node created next. */

                    CommentNode = AcpiOsAcquireObject (AcpiGbl_RegCommentCache);
                    CommentNode->Comment = ACPI_CAST_PTR (char, Aml);
                    CommentNode->Next = NULL;

                    if (!AcpiGbl_DefBlkCommentListHead)
                    {
                        AcpiGbl_DefBlkCommentListHead = CommentNode;
                        AcpiGbl_DefBlkCommentListTail = CommentNode;
                    }
                    else
                    {
                        AcpiGbl_DefBlkCommentListTail->Next = CommentNode;
                        AcpiGbl_DefBlkCommentListTail = AcpiGbl_DefBlkCommentListTail->Next;
                    }
                    break;

                case STANDARD_COMMENT:

                    CvDbgPrint ("found regular comment.\n");

                    /* add to a linked list of nodes. This list will be taken by the parse node created next. */

                    CommentNode = AcpiOsAcquireObject (AcpiGbl_RegCommentCache);
                    CommentNode->Comment = ACPI_CAST_PTR (char, Aml);
                    CommentNode->Next    = NULL;

                    if (!AcpiGbl_RegCommentListHead)
                    {
                        AcpiGbl_RegCommentListHead = CommentNode;
                        AcpiGbl_RegCommentListTail = CommentNode;
                    }
                    else
                    {
                        AcpiGbl_RegCommentListTail->Next = CommentNode;
                        AcpiGbl_RegCommentListTail = AcpiGbl_RegCommentListTail->Next;
                    }
                    break;

                case ENDBLK_COMMENT:

                    CvDbgPrint ("found endblk comment.\n");

                    /* add to a linked list of nodes. This will be taken by the next created parse node. */

                    CommentNode = AcpiOsAcquireObject (AcpiGbl_RegCommentCache);
                    CommentNode->Comment = ACPI_CAST_PTR (char, Aml);
                    CommentNode->Next    = NULL;

                    if (!AcpiGbl_EndBlkCommentListHead)
                    {
                        AcpiGbl_EndBlkCommentListHead = CommentNode;
                        AcpiGbl_EndBlkCommentListTail = CommentNode;
                    }
                    else
                    {
                        AcpiGbl_EndBlkCommentListTail->Next = CommentNode;
                        AcpiGbl_EndBlkCommentListTail = AcpiGbl_EndBlkCommentListTail->Next;
                    }
                    break;

                case INLINE_COMMENT:

                    CvDbgPrint ("found inline comment.\n");
                    AcpiGbl_CurrentInlineComment = ACPI_CAST_PTR (char, Aml);
                    break;

                case ENDNODE_COMMENT:

                    CvDbgPrint ("found EndNode comment.\n");
                    AcpiGbl_CurrentEndNodeComment = ACPI_CAST_PTR (char, Aml);
                    break;

                case CLOSE_BRACE_COMMENT:

                    CvDbgPrint ("found close brace comment.\n");
                    AcpiGbl_CurrentCloseBraceComment = ACPI_CAST_PTR (char, Aml);
                    break;

                case END_DEFBLK_COMMENT:

                    CvDbgPrint ("Found comment that belongs after the } for a definition block.\n");
                    AcpiGbl_CurrentScope->Common.CloseBraceComment = ACPI_CAST_PTR (char, Aml);
                    break;

                case FILENAME_COMMENT:

                    CvDbgPrint ("Found a filename: %s\n", ACPI_CAST_PTR (char, Aml));
                    FileNode = CvFilenameExists (ACPI_CAST_PTR (char, Aml), AcpiGbl_FileTreeRoot);

                    /*
                     * If there is an INCLUDE_COMMENT followed by a
                     * FILENAME_COMMENT, then the INCLUDE_COMMENT is a comment
                     * that is emitted before the #include for the file.
                     * We will save the IncludeComment within the FileNode
                     * associated with this FILENAME_COMMENT.
                     */
                    if (FileNode && AcpiGbl_IncCommentListHead)
                    {
                        FileNode->IncludeComment = AcpiGbl_IncCommentListHead;
                        AcpiGbl_IncCommentListHead = NULL;
                        AcpiGbl_IncCommentListTail = NULL;
                    }
                    break;

                case PARENTFILENAME_COMMENT:
                    CvDbgPrint ("    Found a parent filename.\n");
                    break;

                case INCLUDE_COMMENT:

                    /*
                     * Add to a linked list. This list will be taken by the
                     * parse node created next. See the FILENAME_COMMENT case
                     * for more details
                     */
                    CommentNode = AcpiOsAcquireObject (AcpiGbl_RegCommentCache);
                    CommentNode->Comment = ACPI_CAST_PTR (char, Aml);
                    CommentNode->Next = NULL;

                    if (!AcpiGbl_IncCommentListHead)
                    {
                        AcpiGbl_IncCommentListHead = CommentNode;
                        AcpiGbl_IncCommentListTail = CommentNode;
                    }
                    else
                    {
                        AcpiGbl_IncCommentListTail->Next = CommentNode;
                        AcpiGbl_IncCommentListTail = AcpiGbl_IncCommentListTail->Next;
                    }

                    CvDbgPrint ("Found a include comment: %s\n", CommentNode->Comment);
                    break;

                default:

                    /* Not a valid comment option. Revert the AML */

                    Aml -= 2;
                    goto DefBlock;
                    break;

            } /* end switch statement */

        } /* end else */

        /* determine the length and move forward that amount */

        Length = 0;
        while (ParserState->Aml[Length])
        {
            Length++;
        }

        ParserState->Aml += Length + 1;


        /* Peek at the next Opcode. */

        Aml = ParserState->Aml;
        Opcode = (UINT16) ACPI_GET8 (Aml);

    }

DefBlock:
    if (StdDefBlockFlag)
    {
        /*
         * Give all of its comments to the current scope, which is known as
         * the definition block, since STD_DEFBLK_COMMENT only appears after
         * definition block headers.
         */
        AcpiGbl_CurrentScope->Common.CommentList
            = AcpiGbl_DefBlkCommentListHead;
        AcpiGbl_DefBlkCommentListHead = NULL;
        AcpiGbl_DefBlkCommentListTail = NULL;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvCaptureComments
 *
 * PARAMETERS:  ParserState         - A parser state object
 *
 * RETURN:      none
 *
 * DESCRIPTION: Wrapper function for CvCaptureCommentsOnly
 *              This is referred as ASL_CV_CAPTURE_COMMENTS.
 *
 ******************************************************************************/

void
CvCaptureComments (
    ACPI_WALK_STATE         *WalkState)
{
    UINT8                   *Aml;
    UINT16                  Opcode;
    const ACPI_OPCODE_INFO  *OpInfo;


    if (!Gbl_CaptureComments)
    {
        return;
    }

    /*
     * Before parsing, check to see that comments that come directly after
     * deferred opcodes aren't being processed.
     */
    Aml = WalkState->ParserState.Aml;
    Opcode = (UINT16) ACPI_GET8 (Aml);
    OpInfo = AcpiPsGetOpcodeInfo (Opcode);

    if (!(OpInfo->Flags & AML_DEFER) ||
        ((OpInfo->Flags & AML_DEFER) &&
        (WalkState->PassNumber != ACPI_IMODE_LOAD_PASS1)))
    {
        CvCaptureCommentsOnly (&WalkState->ParserState);
        WalkState->Aml = WalkState->ParserState.Aml;
    }

}


/*******************************************************************************
 *
 * FUNCTION:    CvTransferComments
 *
 * PARAMETERS:  Op    - Transfer comments to this Op
 *
 * RETURN:      none
 *
 * DESCRIPTION: Transfer all of the commments stored in global containers to the
 *              given Op. This will be invoked shortly after the parser creates
 *              a ParseOp.
 *              This is referred as ASL_CV_TRANSFER_COMMENTS.
 *
 ******************************************************************************/

void
CvTransferComments (
    ACPI_PARSE_OBJECT       *Op)
{
    Op->Common.InlineComment = AcpiGbl_CurrentInlineComment;
    AcpiGbl_CurrentInlineComment = NULL;

    Op->Common.EndNodeComment = AcpiGbl_CurrentEndNodeComment;
    AcpiGbl_CurrentEndNodeComment = NULL;

    Op->Common.CloseBraceComment = AcpiGbl_CurrentCloseBraceComment;
    AcpiGbl_CurrentCloseBraceComment = NULL;

    Op->Common.CommentList = AcpiGbl_RegCommentListHead;
    AcpiGbl_RegCommentListHead = NULL;
    AcpiGbl_RegCommentListTail = NULL;

    Op->Common.EndBlkComment = AcpiGbl_EndBlkCommentListHead;
    AcpiGbl_EndBlkCommentListHead = NULL;
    AcpiGbl_EndBlkCommentListTail = NULL;

}
