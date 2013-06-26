/******************************************************************************
 *
 * Module Name: asltree - parse tree management
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


#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "acapps.h"
#include <time.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asltree")

/* Local prototypes */

static ACPI_PARSE_OBJECT *
TrGetNextNode (
    void);

static char *
TrGetNodeFlagName (
    UINT32                  Flags);


/*******************************************************************************
 *
 * FUNCTION:    TrGetNextNode
 *
 * PARAMETERS:  None
 *
 * RETURN:      New parse node. Aborts on allocation failure
 *
 * DESCRIPTION: Allocate a new parse node for the parse tree. Bypass the local
 *              dynamic memory manager for performance reasons (This has a
 *              major impact on the speed of the compiler.)
 *
 ******************************************************************************/

static ACPI_PARSE_OBJECT *
TrGetNextNode (
    void)
{

    if (Gbl_NodeCacheNext >= Gbl_NodeCacheLast)
    {
        Gbl_NodeCacheNext = UtLocalCalloc (sizeof (ACPI_PARSE_OBJECT) *
                                ASL_NODE_CACHE_SIZE);
        Gbl_NodeCacheLast = Gbl_NodeCacheNext + ASL_NODE_CACHE_SIZE;
    }

    return (Gbl_NodeCacheNext++);
}


/*******************************************************************************
 *
 * FUNCTION:    TrAllocateNode
 *
 * PARAMETERS:  ParseOpcode         - Opcode to be assigned to the node
 *
 * RETURN:      New parse node. Aborts on allocation failure
 *
 * DESCRIPTION: Allocate and initialize a new parse node for the parse tree
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrAllocateNode (
    UINT32                  ParseOpcode)
{
    ACPI_PARSE_OBJECT       *Op;


    Op = TrGetNextNode ();

    Op->Asl.ParseOpcode       = (UINT16) ParseOpcode;
    Op->Asl.Filename          = Gbl_Files[ASL_FILE_INPUT].Filename;
    Op->Asl.LineNumber        = Gbl_CurrentLineNumber;
    Op->Asl.LogicalLineNumber = Gbl_LogicalLineNumber;
    Op->Asl.LogicalByteOffset = Gbl_CurrentLineOffset;
    Op->Asl.Column            = Gbl_CurrentColumn;

    UtSetParseOpName (Op);
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrReleaseNode
 *
 * PARAMETERS:  Op            - Op to be released
 *
 * RETURN:      None
 *
 * DESCRIPTION: "release" a node. In truth, nothing is done since the node
 *              is part of a larger buffer
 *
 ******************************************************************************/

void
TrReleaseNode (
    ACPI_PARSE_OBJECT       *Op)
{

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    TrUpdateNode
 *
 * PARAMETERS:  ParseOpcode         - New opcode to be assigned to the node
 *              Op                - An existing parse node
 *
 * RETURN:      The updated node
 *
 * DESCRIPTION: Change the parse opcode assigned to a node. Usually used to
 *              change an opcode to DEFAULT_ARG so that the node is ignored
 *              during the code generation. Also used to set generic integers
 *              to a specific size (8, 16, 32, or 64 bits)
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrUpdateNode (
    UINT32                  ParseOpcode,
    ACPI_PARSE_OBJECT       *Op)
{

    if (!Op)
    {
        return (NULL);
    }

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nUpdateNode: Old - %s, New - %s\n\n",
        UtGetOpName (Op->Asl.ParseOpcode),
        UtGetOpName (ParseOpcode));

    /* Assign new opcode and name */

    if (Op->Asl.ParseOpcode == PARSEOP_ONES)
    {
        switch (ParseOpcode)
        {
        case PARSEOP_BYTECONST:

            Op->Asl.Value.Integer = ACPI_UINT8_MAX;
            break;

        case PARSEOP_WORDCONST:

            Op->Asl.Value.Integer = ACPI_UINT16_MAX;
            break;

        case PARSEOP_DWORDCONST:

            Op->Asl.Value.Integer = ACPI_UINT32_MAX;
            break;

        /* Don't need to do the QWORD case */

        default:

            /* Don't care about others */
            break;
        }
    }

    Op->Asl.ParseOpcode = (UINT16) ParseOpcode;
    UtSetParseOpName (Op);

    /*
     * For the BYTE, WORD, and DWORD constants, make sure that the integer
     * that was passed in will actually fit into the data type
     */
    switch (ParseOpcode)
    {
    case PARSEOP_BYTECONST:

        UtCheckIntegerRange (Op, 0x00, ACPI_UINT8_MAX);
        Op->Asl.Value.Integer &= ACPI_UINT8_MAX;
        break;

    case PARSEOP_WORDCONST:

        UtCheckIntegerRange (Op, 0x00, ACPI_UINT16_MAX);
        Op->Asl.Value.Integer &= ACPI_UINT16_MAX;
        break;

    case PARSEOP_DWORDCONST:

        UtCheckIntegerRange (Op, 0x00, ACPI_UINT32_MAX);
        Op->Asl.Value.Integer &= ACPI_UINT32_MAX;
        break;

    default:

        /* Don't care about others, don't need to check QWORD */

        break;
    }

    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrGetNodeFlagName
 *
 * PARAMETERS:  Flags               - Flags word to be decoded
 *
 * RETURN:      Name string. Always returns a valid string pointer.
 *
 * DESCRIPTION: Decode a flags word
 *
 ******************************************************************************/

static char *
TrGetNodeFlagName (
    UINT32                  Flags)
{

    switch (Flags)
    {
    case NODE_VISITED:

        return ("NODE_VISITED");

    case NODE_AML_PACKAGE:

        return ("NODE_AML_PACKAGE");

    case NODE_IS_TARGET:

        return ("NODE_IS_TARGET");

    case NODE_IS_RESOURCE_DESC:

        return ("NODE_IS_RESOURCE_DESC");

    case NODE_IS_RESOURCE_FIELD:

        return ("NODE_IS_RESOURCE_FIELD");

    case NODE_HAS_NO_EXIT:

        return ("NODE_HAS_NO_EXIT");

    case NODE_IF_HAS_NO_EXIT:

        return ("NODE_IF_HAS_NO_EXIT");

    case NODE_NAME_INTERNALIZED:

        return ("NODE_NAME_INTERNALIZED");

    case NODE_METHOD_NO_RETVAL:

        return ("NODE_METHOD_NO_RETVAL");

    case NODE_METHOD_SOME_NO_RETVAL:

        return ("NODE_METHOD_SOME_NO_RETVAL");

    case NODE_RESULT_NOT_USED:

        return ("NODE_RESULT_NOT_USED");

    case NODE_METHOD_TYPED:

        return ("NODE_METHOD_TYPED");

    case NODE_COMPILE_TIME_CONST:

        return ("NODE_COMPILE_TIME_CONST");

    case NODE_IS_TERM_ARG:

        return ("NODE_IS_TERM_ARG");

    case NODE_WAS_ONES_OP:

        return ("NODE_WAS_ONES_OP");

    case NODE_IS_NAME_DECLARATION:

        return ("NODE_IS_NAME_DECLARATION");

    default:

        return ("Multiple Flags (or unknown flag) set");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetNodeFlags
 *
 * PARAMETERS:  Op                  - An existing parse node
 *              Flags               - New flags word
 *
 * RETURN:      The updated parser op
 *
 * DESCRIPTION: Set bits in the node flags word. Will not clear bits, only set
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrSetNodeFlags (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Flags)
{

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nSetNodeFlags: Op %p, %8.8X %s\n\n", Op, Flags,
        TrGetNodeFlagName (Flags));

    if (!Op)
    {
        return (NULL);
    }

    Op->Asl.CompileFlags |= Flags;
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetNodeAmlLength
 *
 * PARAMETERS:  Op                  - An existing parse node
 *              Length              - AML Length
 *
 * RETURN:      The updated parser op
 *
 * DESCRIPTION: Set the AML Length in a node. Used by the parser to indicate
 *              the presence of a node that must be reduced to a fixed length
 *              constant.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrSetNodeAmlLength (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Length)
{

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nSetNodeAmlLength: Op %p, %8.8X\n", Op, Length);

    if (!Op)
    {
        return (NULL);
    }

    Op->Asl.AmlLength = Length;
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetEndLineNumber
 *
 * PARAMETERS:  Op                - An existing parse node
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Set the ending line numbers (file line and logical line) of a
 *              parse node to the current line numbers.
 *
 ******************************************************************************/

void
TrSetEndLineNumber (
    ACPI_PARSE_OBJECT       *Op)
{

    /* If the end line # is already set, just return */

    if (Op->Asl.EndLine)
    {
        return;
    }

    Op->Asl.EndLine        = Gbl_CurrentLineNumber;
    Op->Asl.EndLogicalLine = Gbl_LogicalLineNumber;
}


/*******************************************************************************
 *
 * FUNCTION:    TrCreateLeafNode
 *
 * PARAMETERS:  ParseOpcode         - New opcode to be assigned to the node
 *
 * RETURN:      Pointer to the new node. Aborts on allocation failure
 *
 * DESCRIPTION: Create a simple leaf node (no children or peers, and no value
 *              assigned to the node)
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrCreateLeafNode (
    UINT32                  ParseOpcode)
{
    ACPI_PARSE_OBJECT       *Op;


    Op = TrAllocateNode (ParseOpcode);

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nCreateLeafNode  Ln/Col %u/%u NewNode %p  Op %s\n\n",
        Op->Asl.LineNumber, Op->Asl.Column, Op, UtGetOpName(ParseOpcode));

    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrCreateConstantLeafNode
 *
 * PARAMETERS:  ParseOpcode         - The constant opcode
 *
 * RETURN:      Pointer to the new node. Aborts on allocation failure
 *
 * DESCRIPTION: Create a leaf node (no children or peers) for one of the
 *              special constants - __LINE__, __FILE__, and __DATE__.
 *
 * Note: An implemenation of __FUNC__ cannot happen here because we don't
 * have a full parse tree at this time and cannot find the parent control
 * method. If it is ever needed, __FUNC__ must be implemented later, after
 * the parse tree has been fully constructed.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrCreateConstantLeafNode (
    UINT32                  ParseOpcode)
{
    ACPI_PARSE_OBJECT       *Op = NULL;
    time_t                  CurrentTime;
    char                    *StaticTimeString;
    char                    *TimeString;
    char                    *Path;
    char                    *Filename;


    switch (ParseOpcode)
    {
    case PARSEOP___LINE__:

        Op = TrAllocateNode (PARSEOP_INTEGER);
        Op->Asl.Value.Integer = Op->Asl.LineNumber;
        break;

    case PARSEOP___PATH__:

        Op = TrAllocateNode (PARSEOP_STRING_LITERAL);

        /* Op.Asl.Filename contains the full pathname to the file */

        Op->Asl.Value.String = Op->Asl.Filename;
        break;

    case PARSEOP___FILE__:

        Op = TrAllocateNode (PARSEOP_STRING_LITERAL);

        /* Get the simple filename from the full path */

        FlSplitInputPathname (Op->Asl.Filename, &Path, &Filename);
        ACPI_FREE (Path);
        Op->Asl.Value.String = Filename;
        break;

    case PARSEOP___DATE__:

        Op = TrAllocateNode (PARSEOP_STRING_LITERAL);

        /* Get a copy of the current time */

        CurrentTime = time (NULL);
        StaticTimeString = ctime (&CurrentTime);
        TimeString = UtLocalCalloc (strlen (StaticTimeString) + 1);
        strcpy (TimeString, StaticTimeString);

        TimeString[strlen(TimeString) -1] = 0;  /* Remove trailing newline */
        Op->Asl.Value.String = TimeString;
        break;

    default: /* This would be an internal error */

        return (NULL);
    }

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nCreateConstantLeafNode  Ln/Col %u/%u NewNode %p  Op %s  Value %8.8X%8.8X  ",
        Op->Asl.LineNumber, Op->Asl.Column, Op, UtGetOpName (ParseOpcode),
        ACPI_FORMAT_UINT64 (Op->Asl.Value.Integer));
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrCreateValuedLeafNode
 *
 * PARAMETERS:  ParseOpcode         - New opcode to be assigned to the node
 *              Value               - Value to be assigned to the node
 *
 * RETURN:      Pointer to the new node. Aborts on allocation failure
 *
 * DESCRIPTION: Create a leaf node (no children or peers) with a value
 *              assigned to it
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrCreateValuedLeafNode (
    UINT32                  ParseOpcode,
    UINT64                  Value)
{
    ACPI_PARSE_OBJECT       *Op;


    Op = TrAllocateNode (ParseOpcode);

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nCreateValuedLeafNode  Ln/Col %u/%u NewNode %p  Op %s  Value %8.8X%8.8X  ",
        Op->Asl.LineNumber, Op->Asl.Column, Op, UtGetOpName(ParseOpcode),
        ACPI_FORMAT_UINT64 (Value));
    Op->Asl.Value.Integer = Value;

    switch (ParseOpcode)
    {
    case PARSEOP_STRING_LITERAL:

        DbgPrint (ASL_PARSE_OUTPUT, "STRING->%s", Value);
        break;

    case PARSEOP_NAMESEG:

        DbgPrint (ASL_PARSE_OUTPUT, "NAMESEG->%s", Value);
        break;

    case PARSEOP_NAMESTRING:

        DbgPrint (ASL_PARSE_OUTPUT, "NAMESTRING->%s", Value);
        break;

    case PARSEOP_EISAID:

        DbgPrint (ASL_PARSE_OUTPUT, "EISAID->%s", Value);
        break;

    case PARSEOP_METHOD:

        DbgPrint (ASL_PARSE_OUTPUT, "METHOD");
        break;

    case PARSEOP_INTEGER:

        DbgPrint (ASL_PARSE_OUTPUT, "INTEGER");
        break;

    default:

        break;
    }

    DbgPrint (ASL_PARSE_OUTPUT, "\n\n");
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrCreateNode
 *
 * PARAMETERS:  ParseOpcode         - Opcode to be assigned to the node
 *              NumChildren         - Number of children to follow
 *              ...                 - A list of child nodes to link to the new
 *                                    node. NumChildren long.
 *
 * RETURN:      Pointer to the new node. Aborts on allocation failure
 *
 * DESCRIPTION: Create a new parse node and link together a list of child
 *              nodes underneath the new node.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrCreateNode (
    UINT32                  ParseOpcode,
    UINT32                  NumChildren,
    ...)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_PARSE_OBJECT       *Child;
    ACPI_PARSE_OBJECT       *PrevChild;
    va_list                 ap;
    UINT32                  i;
    BOOLEAN                 FirstChild;


    va_start (ap, NumChildren);

    /* Allocate one new node */

    Op = TrAllocateNode (ParseOpcode);

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nCreateNode  Ln/Col %u/%u NewParent %p Child %u Op %s  ",
        Op->Asl.LineNumber, Op->Asl.Column, Op, NumChildren, UtGetOpName(ParseOpcode));

    /* Some extra debug output based on the parse opcode */

    switch (ParseOpcode)
    {
    case PARSEOP_DEFINITIONBLOCK:

        RootNode = Op;
        DbgPrint (ASL_PARSE_OUTPUT, "DEFINITION_BLOCK (Tree Completed)->");
        break;

    case PARSEOP_OPERATIONREGION:

        DbgPrint (ASL_PARSE_OUTPUT, "OPREGION->");
        break;

    case PARSEOP_OR:

        DbgPrint (ASL_PARSE_OUTPUT, "OR->");
        break;

    default:

        /* Nothing to do for other opcodes */

        break;
    }

    /* Link the new node to its children */

    PrevChild = NULL;
    FirstChild = TRUE;
    for (i = 0; i < NumChildren; i++)
    {
        /* Get the next child */

        Child = va_arg (ap, ACPI_PARSE_OBJECT *);
        DbgPrint (ASL_PARSE_OUTPUT, "%p, ", Child);

        /*
         * If child is NULL, this means that an optional argument
         * was omitted. We must create a placeholder with a special
         * opcode (DEFAULT_ARG) so that the code generator will know
         * that it must emit the correct default for this argument
         */
        if (!Child)
        {
            Child = TrAllocateNode (PARSEOP_DEFAULT_ARG);
        }

        /* Link first child to parent */

        if (FirstChild)
        {
            FirstChild = FALSE;
            Op->Asl.Child = Child;
        }

        /* Point all children to parent */

        Child->Asl.Parent = Op;

        /* Link children in a peer list */

        if (PrevChild)
        {
            PrevChild->Asl.Next = Child;
        };

        /*
         * This child might be a list, point all nodes in the list
         * to the same parent
         */
        while (Child->Asl.Next)
        {
            Child = Child->Asl.Next;
            Child->Asl.Parent = Op;
        }

        PrevChild = Child;
    }
    va_end(ap);

    DbgPrint (ASL_PARSE_OUTPUT, "\n\n");
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkChildren
 *
 * PARAMETERS:  Op                - An existing parse node
 *              NumChildren         - Number of children to follow
 *              ...                 - A list of child nodes to link to the new
 *                                    node. NumChildren long.
 *
 * RETURN:      The updated (linked) node
 *
 * DESCRIPTION: Link a group of nodes to an existing parse node
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkChildren (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  NumChildren,
    ...)
{
    ACPI_PARSE_OBJECT       *Child;
    ACPI_PARSE_OBJECT       *PrevChild;
    va_list                 ap;
    UINT32                  i;
    BOOLEAN                 FirstChild;


    va_start (ap, NumChildren);


    TrSetEndLineNumber (Op);

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkChildren  Line [%u to %u] NewParent %p Child %u Op %s  ",
        Op->Asl.LineNumber, Op->Asl.EndLine,
        Op, NumChildren, UtGetOpName(Op->Asl.ParseOpcode));

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_DEFINITIONBLOCK:

        RootNode = Op;
        DbgPrint (ASL_PARSE_OUTPUT, "DEFINITION_BLOCK (Tree Completed)->");
        break;

    case PARSEOP_OPERATIONREGION:

        DbgPrint (ASL_PARSE_OUTPUT, "OPREGION->");
        break;

    case PARSEOP_OR:

        DbgPrint (ASL_PARSE_OUTPUT, "OR->");
        break;

    default:

        /* Nothing to do for other opcodes */

        break;
    }

    /* Link the new node to it's children */

    PrevChild = NULL;
    FirstChild = TRUE;
    for (i = 0; i < NumChildren; i++)
    {
        Child = va_arg (ap, ACPI_PARSE_OBJECT *);

        if ((Child == PrevChild) && (Child != NULL))
        {
            AslError (ASL_WARNING, ASL_MSG_COMPILER_INTERNAL, Child,
                "Child node list invalid");
            va_end(ap);
            return (Op);
        }

        DbgPrint (ASL_PARSE_OUTPUT, "%p, ", Child);

        /*
         * If child is NULL, this means that an optional argument
         * was omitted. We must create a placeholder with a special
         * opcode (DEFAULT_ARG) so that the code generator will know
         * that it must emit the correct default for this argument
         */
        if (!Child)
        {
            Child = TrAllocateNode (PARSEOP_DEFAULT_ARG);
        }

        /* Link first child to parent */

        if (FirstChild)
        {
            FirstChild = FALSE;
            Op->Asl.Child = Child;
        }

        /* Point all children to parent */

        Child->Asl.Parent = Op;

        /* Link children in a peer list */

        if (PrevChild)
        {
            PrevChild->Asl.Next = Child;
        };

        /*
         * This child might be a list, point all nodes in the list
         * to the same parent
         */
        while (Child->Asl.Next)
        {
            Child = Child->Asl.Next;
            Child->Asl.Parent = Op;
        }
        PrevChild = Child;
    }

    va_end(ap);
    DbgPrint (ASL_PARSE_OUTPUT, "\n\n");
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkPeerNode
 *
 * PARAMETERS:  Op1           - First peer
 *              Op2           - Second peer
 *
 * RETURN:      Op1 or the non-null node.
 *
 * DESCRIPTION: Link two nodes as peers. Handles cases where one peer is null.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkPeerNode (
    ACPI_PARSE_OBJECT       *Op1,
    ACPI_PARSE_OBJECT       *Op2)
{
    ACPI_PARSE_OBJECT       *Next;


    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkPeerNode: 1=%p (%s), 2=%p (%s)\n\n",
        Op1, Op1 ? UtGetOpName(Op1->Asl.ParseOpcode) : NULL,
        Op2, Op2 ? UtGetOpName(Op2->Asl.ParseOpcode) : NULL);


    if ((!Op1) && (!Op2))
    {
        DbgPrint (ASL_PARSE_OUTPUT, "\nTwo Null nodes!\n");
        return (Op1);
    }

    /* If one of the nodes is null, just return the non-null node */

    if (!Op2)
    {
        return (Op1);
    }

    if (!Op1)
    {
        return (Op2);
    }

    if (Op1 == Op2)
    {
        DbgPrint (ASL_DEBUG_OUTPUT,
            "\n\n************* Internal error, linking node to itself %p\n\n\n",
            Op1);
        AslError (ASL_WARNING, ASL_MSG_COMPILER_INTERNAL, Op1,
            "Linking node to itself");
        return (Op1);
    }

    Op1->Asl.Parent = Op2->Asl.Parent;

    /*
     * Op 1 may already have a peer list (such as an IF/ELSE pair),
     * so we must walk to the end of the list and attach the new
     * peer at the end
     */
    Next = Op1;
    while (Next->Asl.Next)
    {
        Next = Next->Asl.Next;
    }

    Next->Asl.Next = Op2;
    return (Op1);
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkPeerNodes
 *
 * PARAMETERS:  NumPeers            - The number of nodes in the list to follow
 *              ...                 - A list of nodes to link together as peers
 *
 * RETURN:      The first node in the list (head of the peer list)
 *
 * DESCRIPTION: Link together an arbitrary number of peer nodes.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkPeerNodes (
    UINT32                  NumPeers,
    ...)
{
    ACPI_PARSE_OBJECT       *This;
    ACPI_PARSE_OBJECT       *Next;
    va_list                 ap;
    UINT32                  i;
    ACPI_PARSE_OBJECT       *Start;


    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkPeerNodes: (%u) ", NumPeers);

    va_start (ap, NumPeers);
    This = va_arg (ap, ACPI_PARSE_OBJECT *);
    Start = This;

    /*
     * Link all peers
     */
    for (i = 0; i < (NumPeers -1); i++)
    {
        DbgPrint (ASL_PARSE_OUTPUT, "%u=%p ", (i+1), This);

        while (This->Asl.Next)
        {
            This = This->Asl.Next;
        }

        /* Get another peer node */

        Next = va_arg (ap, ACPI_PARSE_OBJECT *);
        if (!Next)
        {
            Next = TrAllocateNode (PARSEOP_DEFAULT_ARG);
        }

        /* link new node to the current node */

        This->Asl.Next = Next;
        This = Next;
    }
    va_end (ap);

    DbgPrint (ASL_PARSE_OUTPUT,"\n\n");
    return (Start);
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkChildNode
 *
 * PARAMETERS:  Op1           - Parent node
 *              Op2           - Op to become a child
 *
 * RETURN:      The parent node
 *
 * DESCRIPTION: Link two nodes together as a parent and child
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkChildNode (
    ACPI_PARSE_OBJECT       *Op1,
    ACPI_PARSE_OBJECT       *Op2)
{
    ACPI_PARSE_OBJECT       *Next;


    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkChildNode: Parent=%p (%s), Child=%p (%s)\n\n",
        Op1, Op1 ? UtGetOpName(Op1->Asl.ParseOpcode): NULL,
        Op2, Op2 ? UtGetOpName(Op2->Asl.ParseOpcode): NULL);

    if (!Op1 || !Op2)
    {
        return (Op1);
    }

    Op1->Asl.Child = Op2;

    /* Set the child and all peers of the child to point to the parent */

    Next = Op2;
    while (Next)
    {
        Next->Asl.Parent = Op1;
        Next = Next->Asl.Next;
    }

    return (Op1);
}


/*******************************************************************************
 *
 * FUNCTION:    TrWalkParseTree
 *
 * PARAMETERS:  Visitation              - Type of walk
 *              DescendingCallback      - Called during tree descent
 *              AscendingCallback       - Called during tree ascent
 *              Context                 - To be passed to the callbacks
 *
 * RETURN:      Status from callback(s)
 *
 * DESCRIPTION: Walk the entire parse tree.
 *
 ******************************************************************************/

ACPI_STATUS
TrWalkParseTree (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Visitation,
    ASL_WALK_CALLBACK       DescendingCallback,
    ASL_WALK_CALLBACK       AscendingCallback,
    void                    *Context)
{
    UINT32                  Level;
    BOOLEAN                 NodePreviouslyVisited;
    ACPI_PARSE_OBJECT       *StartOp = Op;
    ACPI_STATUS             Status;


    if (!RootNode)
    {
        return (AE_OK);
    }

    Level = 0;
    NodePreviouslyVisited = FALSE;

    switch (Visitation)
    {
    case ASL_WALK_VISIT_DOWNWARD:

        while (Op)
        {
            if (!NodePreviouslyVisited)
            {
                /* Let the callback process the node. */

                Status = DescendingCallback (Op, Level, Context);
                if (ACPI_SUCCESS (Status))
                {
                    /* Visit children first, once */

                    if (Op->Asl.Child)
                    {
                        Level++;
                        Op = Op->Asl.Child;
                        continue;
                    }
                }
                else if (Status != AE_CTRL_DEPTH)
                {
                    /* Exit immediately on any error */

                    return (Status);
                }
            }

            /* Terminate walk at start op */

            if (Op == StartOp)
            {
                break;
            }

            /* No more children, visit peers */

            if (Op->Asl.Next)
            {
                Op = Op->Asl.Next;
                NodePreviouslyVisited = FALSE;
            }
            else
            {
                /* No children or peers, re-visit parent */

                if (Level != 0 )
                {
                    Level--;
                }
                Op = Op->Asl.Parent;
                NodePreviouslyVisited = TRUE;
            }
        }
        break;

    case ASL_WALK_VISIT_UPWARD:

        while (Op)
        {
            /* Visit leaf node (no children) or parent node on return trip */

            if ((!Op->Asl.Child) ||
                (NodePreviouslyVisited))
            {
                /* Let the callback process the node. */

                Status = AscendingCallback (Op, Level, Context);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
            }
            else
            {
                /* Visit children first, once */

                Level++;
                Op = Op->Asl.Child;
                continue;
            }

            /* Terminate walk at start op */

            if (Op == StartOp)
            {
                break;
            }

            /* No more children, visit peers */

            if (Op->Asl.Next)
            {
                Op = Op->Asl.Next;
                NodePreviouslyVisited = FALSE;
            }
            else
            {
                /* No children or peers, re-visit parent */

                if (Level != 0 )
                {
                    Level--;
                }
                Op = Op->Asl.Parent;
                NodePreviouslyVisited = TRUE;
            }
        }
        break;

     case ASL_WALK_VISIT_TWICE:

        while (Op)
        {
            if (NodePreviouslyVisited)
            {
                Status = AscendingCallback (Op, Level, Context);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
            }
            else
            {
                /* Let the callback process the node. */

                Status = DescendingCallback (Op, Level, Context);
                if (ACPI_SUCCESS (Status))
                {
                    /* Visit children first, once */

                    if (Op->Asl.Child)
                    {
                        Level++;
                        Op = Op->Asl.Child;
                        continue;
                    }
                }
                else if (Status != AE_CTRL_DEPTH)
                {
                    /* Exit immediately on any error */

                    return (Status);
                }
            }

            /* Terminate walk at start op */

            if (Op == StartOp)
            {
                break;
            }

            /* No more children, visit peers */

            if (Op->Asl.Next)
            {
                Op = Op->Asl.Next;
                NodePreviouslyVisited = FALSE;
            }
            else
            {
                /* No children or peers, re-visit parent */

                if (Level != 0 )
                {
                    Level--;
                }
                Op = Op->Asl.Parent;
                NodePreviouslyVisited = TRUE;
            }
        }
        break;

    default:
        /* No other types supported */
        break;
    }

    /* If we get here, the walk completed with no errors */

    return (AE_OK);
}
