/******************************************************************************
 *
 * Module Name: adwalk - Application-level disassembler parse tree walk routines
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


#include "acpi.h"
#include "accommon.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdisasm.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acapps.h"


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("adwalk")

/*
 * aslmap - opcode mappings and reserved method names
 */
ACPI_OBJECT_TYPE
AslMapNamedOpcodeToDataType (
    UINT16                  Opcode);

/* Local prototypes */

static ACPI_STATUS
AcpiDmFindOrphanDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmDumpDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmXrefDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmCommonAscendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmLoadDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static UINT32
AcpiDmInspectPossibleArgs (
    UINT32                  CurrentOpArgCount,
    UINT32                  TargetCount,
    ACPI_PARSE_OBJECT       *Op);

static ACPI_STATUS
AcpiDmResourceDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpTree
 *
 * PARAMETERS:  Origin              - Starting object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse tree walk to format and output the nodes
 *
 ******************************************************************************/

void
AcpiDmDumpTree (
    ACPI_PARSE_OBJECT       *Origin)
{
    ACPI_OP_WALK_INFO       Info;


    if (!Origin)
    {
        return;
    }

    AcpiOsPrintf ("/*\nAML Parse Tree\n\n");
    Info.Flags = 0;
    Info.Count = 0;
    Info.Level = 0;
    Info.WalkState = NULL;
    AcpiDmWalkParseTree (Origin, AcpiDmDumpDescending, NULL, &Info);
    AcpiOsPrintf ("*/\n\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFindOrphanMethods
 *
 * PARAMETERS:  Origin              - Starting object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse tree walk to find "orphaned" method invocations -- methods
 *              that are not resolved in the namespace
 *
 ******************************************************************************/

void
AcpiDmFindOrphanMethods (
    ACPI_PARSE_OBJECT       *Origin)
{
    ACPI_OP_WALK_INFO       Info;


    if (!Origin)
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = NULL;
    AcpiDmWalkParseTree (Origin, AcpiDmFindOrphanDescending, NULL, &Info);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFinishNamespaceLoad
 *
 * PARAMETERS:  ParseTreeRoot       - Root of the parse tree
 *              NamespaceRoot       - Root of the internal namespace
 *              OwnerId             - OwnerId of the table to be disassembled
 *
 * RETURN:      None
 *
 * DESCRIPTION: Load all namespace items that are created within control
 *              methods. Used before namespace cross reference
 *
 ******************************************************************************/

void
AcpiDmFinishNamespaceLoad (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot,
    ACPI_OWNER_ID           OwnerId)
{
    ACPI_STATUS             Status;
    ACPI_OP_WALK_INFO       Info;
    ACPI_WALK_STATE         *WalkState;


    if (!ParseTreeRoot)
    {
        return;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (OwnerId, ParseTreeRoot, NULL, NULL);
    if (!WalkState)
    {
        return;
    }

    Status = AcpiDsScopeStackPush (NamespaceRoot, NamespaceRoot->Type, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = WalkState;
    AcpiDmWalkParseTree (ParseTreeRoot, AcpiDmLoadDescendingOp,
        AcpiDmCommonAscendingOp, &Info);
    ACPI_FREE (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCrossReferenceNamespace
 *
 * PARAMETERS:  ParseTreeRoot       - Root of the parse tree
 *              NamespaceRoot       - Root of the internal namespace
 *              OwnerId             - OwnerId of the table to be disassembled
 *
 * RETURN:      None
 *
 * DESCRIPTION: Cross reference the namespace to create externals
 *
 ******************************************************************************/

void
AcpiDmCrossReferenceNamespace (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot,
    ACPI_OWNER_ID           OwnerId)
{
    ACPI_STATUS             Status;
    ACPI_OP_WALK_INFO       Info;
    ACPI_WALK_STATE         *WalkState;


    if (!ParseTreeRoot)
    {
        return;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (OwnerId, ParseTreeRoot, NULL, NULL);
    if (!WalkState)
    {
        return;
    }

    Status = AcpiDsScopeStackPush (NamespaceRoot, NamespaceRoot->Type, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = WalkState;
    AcpiDmWalkParseTree (ParseTreeRoot, AcpiDmXrefDescendingOp,
        AcpiDmCommonAscendingOp, &Info);
    ACPI_FREE (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmConvertResourceIndexes
 *
 * PARAMETERS:  ParseTreeRoot       - Root of the parse tree
 *              NamespaceRoot       - Root of the internal namespace
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert fixed-offset references to resource descriptors to
 *              symbolic references. Should only be called after namespace has
 *              been cross referenced.
 *
 ******************************************************************************/

void
AcpiDmConvertResourceIndexes (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot)
{
    ACPI_STATUS             Status;
    ACPI_OP_WALK_INFO       Info;
    ACPI_WALK_STATE         *WalkState;


    if (!ParseTreeRoot)
    {
        return;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0, ParseTreeRoot, NULL, NULL);
    if (!WalkState)
    {
        return;
    }

    Status = AcpiDsScopeStackPush (NamespaceRoot, NamespaceRoot->Type, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = WalkState;
    AcpiDmWalkParseTree (ParseTreeRoot, AcpiDmResourceDescendingOp,
        AcpiDmCommonAscendingOp, &Info);
    ACPI_FREE (WalkState);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDescending
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Format and print contents of one parse Op.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmDumpDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    char                    *Path;


    if (!Op)
    {
        return (AE_OK);
    }

    /* Most of the information (count, level, name) here */

    Info->Count++;
    AcpiOsPrintf ("% 5d [%2.2d] ", Info->Count, Level);
    AcpiDmIndent (Level);
    AcpiOsPrintf ("%-28s", AcpiPsGetOpcodeName (Op->Common.AmlOpcode));

    /* Extra info is helpful */

    switch (Op->Common.AmlOpcode)
    {
    case AML_BYTE_OP:

        AcpiOsPrintf ("%2.2X", (UINT32) Op->Common.Value.Integer);
        break;

    case AML_WORD_OP:

        AcpiOsPrintf ("%4.4X", (UINT32) Op->Common.Value.Integer);
        break;

    case AML_DWORD_OP:

        AcpiOsPrintf ("%8.8X", (UINT32) Op->Common.Value.Integer);
        break;

    case AML_QWORD_OP:

        AcpiOsPrintf ("%8.8X%8.8X", ACPI_FORMAT_UINT64 (Op->Common.Value.Integer));
        break;

    case AML_INT_NAMEPATH_OP:

        if (Op->Common.Value.String)
        {
            AcpiNsExternalizeName (ACPI_UINT32_MAX, Op->Common.Value.String,
                            NULL, &Path);
            AcpiOsPrintf ("%s %p", Path, Op->Common.Node);
            ACPI_FREE (Path);
        }
        else
        {
            AcpiOsPrintf ("[NULL]");
        }
        break;

    case AML_NAME_OP:
    case AML_METHOD_OP:
    case AML_DEVICE_OP:
    case AML_INT_NAMEDFIELD_OP:

        AcpiOsPrintf ("%4.4s", ACPI_CAST_PTR (char, &Op->Named.Name));
        break;

    default:

        break;
    }

    AcpiOsPrintf ("\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFindOrphanDescending
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check namepath Ops for orphaned method invocations
 *
 * Note: Experimental.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmFindOrphanDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_PARSE_OBJECT       *ChildOp;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_PARSE_OBJECT       *ParentOp;
    UINT32                  ArgCount;


    if (!Op)
    {
        return (AE_OK);
    }

    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    switch (Op->Common.AmlOpcode)
    {
#ifdef ACPI_UNDER_DEVELOPMENT
    case AML_ADD_OP:

        ChildOp = Op->Common.Value.Arg;
        if ((ChildOp->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
            !ChildOp->Common.Node)
        {
            AcpiNsExternalizeName (ACPI_UINT32_MAX, ChildOp->Common.Value.String,
                NULL, &Path);
            AcpiOsPrintf ("/* %-16s A-NAMEPATH: %s  */\n",
                Op->Common.AmlOpName, Path);
            ACPI_FREE (Path);

            NextOp = Op->Common.Next;
            if (!NextOp)
            {
                /* This NamePath has no args, assume it is an integer */

                AcpiDmAddOpToExternalList (ChildOp,
                    ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0, 0);
                return (AE_OK);
            }

            ArgCount = AcpiDmInspectPossibleArgs (3, 1, NextOp);
            AcpiOsPrintf ("/* A-CHILDREN: %u Actual %u */\n",
                ArgCount, AcpiDmCountChildren (Op));

            if (ArgCount < 1)
            {
                /* One Arg means this is just a Store(Name,Target) */

                AcpiDmAddOpToExternalList (ChildOp,
                    ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0, 0);
                return (AE_OK);
            }

            AcpiDmAddOpToExternalList (ChildOp,
                ChildOp->Common.Value.String, ACPI_TYPE_METHOD, ArgCount, 0);
        }
        break;
#endif

    case AML_STORE_OP:

        ChildOp = Op->Common.Value.Arg;
        if ((ChildOp->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
            !ChildOp->Common.Node)
        {
            NextOp = Op->Common.Next;
            if (!NextOp)
            {
                /* This NamePath has no args, assume it is an integer */

                AcpiDmAddOpToExternalList (ChildOp,
                    ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0, 0);
                return (AE_OK);
            }

            ArgCount = AcpiDmInspectPossibleArgs (2, 1, NextOp);
            if (ArgCount <= 1)
            {
                /* One Arg means this is just a Store(Name,Target) */

                AcpiDmAddOpToExternalList (ChildOp,
                    ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0, 0);
                return (AE_OK);
            }

            AcpiDmAddOpToExternalList (ChildOp,
                ChildOp->Common.Value.String, ACPI_TYPE_METHOD, ArgCount, 0);
        }
        break;

    case AML_INT_NAMEPATH_OP:

        /* Must examine parent to see if this namepath is an argument */

        ParentOp = Op->Common.Parent;
        OpInfo = AcpiPsGetOpcodeInfo (ParentOp->Common.AmlOpcode);

        if ((OpInfo->Class != AML_CLASS_EXECUTE) &&
            (OpInfo->Class != AML_CLASS_CREATE) &&
            (OpInfo->ObjectType != ACPI_TYPE_LOCAL_ALIAS) &&
            (ParentOp->Common.AmlOpcode != AML_INT_METHODCALL_OP) &&
            !Op->Common.Node)
        {
            ArgCount = AcpiDmInspectPossibleArgs (0, 0, Op->Common.Next);

            /*
             * Check if namepath is a predicate for if/while or lone parameter to
             * a return.
             */
            if (ArgCount == 0)
            {
                if (((ParentOp->Common.AmlOpcode == AML_IF_OP) ||
                     (ParentOp->Common.AmlOpcode == AML_WHILE_OP) ||
                     (ParentOp->Common.AmlOpcode == AML_RETURN_OP)) &&

                     /* And namepath is the first argument */
                     (ParentOp->Common.Value.Arg == Op))
                {
                    AcpiDmAddOpToExternalList (Op,
                        Op->Common.Value.String, ACPI_TYPE_INTEGER, 0, 0);
                    break;
                }
            }

            /*
             * This is a standalone namestring (not a parameter to another
             * operator) - it *must* be a method invocation, nothing else is
             * grammatically possible.
             */
            AcpiDmAddOpToExternalList (Op,
                Op->Common.Value.String, ACPI_TYPE_METHOD, ArgCount, 0);
        }
        break;

    default:

        break;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmLoadDescendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending handler for namespace control method object load
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmLoadDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_STATUS             Status;
    char                    *Path = NULL;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_NAMESPACE_NODE     *Node;
    char                    FieldPath[5];
    BOOLEAN                 PreDefined = FALSE;
    UINT8                   PreDefineIndex = 0;


    WalkState = Info->WalkState;
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    ObjectType = OpInfo->ObjectType;
    ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);

    /* Only interested in operators that create new names */

    if (!(OpInfo->Flags & AML_NAMED) &&
        !(OpInfo->Flags & AML_CREATE))
    {
        goto Exit;
    }

    /* Get the NamePath from the appropriate place */

    if (OpInfo->Flags & AML_NAMED)
    {
        /* For all named operators, get the new name */

        Path = (char *) Op->Named.Path;

        if (!Path && Op->Common.AmlOpcode == AML_INT_NAMEDFIELD_OP)
        {
            *ACPI_CAST_PTR (UINT32, &FieldPath[0]) = Op->Named.Name;
            FieldPath[4] = 0;
            Path = FieldPath;
        }
    }
    else if (OpInfo->Flags & AML_CREATE)
    {
        /* New name is the last child */

        NextOp = Op->Common.Value.Arg;

        while (NextOp->Common.Next)
        {
            NextOp = NextOp->Common.Next;
        }
        Path = NextOp->Common.Value.String;
    }

    if (!Path)
    {
        goto Exit;
    }

    /* Insert the name into the namespace */

    Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
                ACPI_IMODE_LOAD_PASS2, ACPI_NS_DONT_OPEN_SCOPE,
                WalkState, &Node);

    Op->Common.Node = Node;

    if (ACPI_SUCCESS (Status))
    {
        /* Check if it's a predefined node */

        while (AcpiGbl_PreDefinedNames[PreDefineIndex].Name)
        {
            if (ACPI_COMPARE_NAME (Node->Name.Ascii,
                AcpiGbl_PreDefinedNames[PreDefineIndex].Name))
            {
                PreDefined = TRUE;
                break;
            }

            PreDefineIndex++;
        }

        /*
         * Set node owner id if it satisfies all the following conditions:
         * 1) Not a predefined node, _SB_ etc
         * 2) Not the root node
         * 3) Not a node created by Scope
         */

        if (!PreDefined && Node != AcpiGbl_RootNode &&
            Op->Common.AmlOpcode != AML_SCOPE_OP)
        {
            Node->OwnerId = WalkState->OwnerId;
        }
    }


Exit:

    if (AcpiNsOpensScope (ObjectType))
    {
        if (Op->Common.Node)
        {
            Status = AcpiDsScopeStackPush (Op->Common.Node, ObjectType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmXrefDescendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending handler for namespace cross reference
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmXrefDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_OBJECT_TYPE        ObjectType2;
    ACPI_STATUS             Status;
    char                    *Path = NULL;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OPERAND_OBJECT     *Object;
    UINT32                  ParamCount = 0;
    char                    *Pathname;


    WalkState = Info->WalkState;
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    ObjectType = OpInfo->ObjectType;
    ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);

    if ((!(OpInfo->Flags & AML_NAMED)) &&
        (!(OpInfo->Flags & AML_CREATE)) &&
        (Op->Common.AmlOpcode != AML_INT_NAMEPATH_OP))
    {
        goto Exit;
    }

    /* Get the NamePath from the appropriate place */

    if (OpInfo->Flags & AML_NAMED)
    {
        /*
         * Only these two operators (Alias, Scope) refer to an existing
         * name, it is the first argument
         */
        if (Op->Common.AmlOpcode == AML_ALIAS_OP)
        {
            ObjectType = ACPI_TYPE_ANY;

            NextOp = Op->Common.Value.Arg;
            NextOp = NextOp->Common.Value.Arg;
            if (NextOp->Common.AmlOpcode == AML_INT_NAMEPATH_OP)
            {
                Path = NextOp->Common.Value.String;
            }
        }
        else if (Op->Common.AmlOpcode == AML_SCOPE_OP)
        {
            Path = (char *) Op->Named.Path;
        }
    }
    else if (OpInfo->Flags & AML_CREATE)
    {
        /* Referenced Buffer Name is the first child */

        ObjectType = ACPI_TYPE_BUFFER; /* Change from TYPE_BUFFER_FIELD */

        NextOp = Op->Common.Value.Arg;
        if (NextOp->Common.AmlOpcode == AML_INT_NAMEPATH_OP)
        {
            Path = NextOp->Common.Value.String;
        }
    }
    else
    {
        Path = Op->Common.Value.String;
    }

    if (!Path)
    {
        goto Exit;
    }

    /*
     * Lookup the name in the namespace. Name must exist at this point, or it
     * is an invalid reference.
     *
     * The namespace is also used as a lookup table for references to resource
     * descriptors and the fields within them.
     */
    Node = NULL;
    Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ACPI_TYPE_ANY,
                ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
                WalkState, &Node);
    if (ACPI_SUCCESS (Status) && (Node->Flags & ANOBJ_IS_EXTERNAL))
    {
        /* Node was created by an External() statement */

        Status = AE_NOT_FOUND;
    }

    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_FOUND)
        {
            /*
             * Add this symbol as an external declaration, except if the
             * parent is a CondRefOf operator. For this operator, we do not
             * need an external, nor do we want one, since this can cause
             * disassembly problems if the symbol is actually a control
             * method.
             */
            if (!(Op->Asl.Parent &&
                (Op->Asl.Parent->Asl.AmlOpcode == AML_COND_REF_OF_OP)))
            {
                if (Node)
                {
                    AcpiDmAddNodeToExternalList (Node,
                        (UINT8) ObjectType, 0, 0);
                }
                else
                {
                    AcpiDmAddOpToExternalList (Op, Path,
                        (UINT8) ObjectType, 0, 0);
                }
            }
        }
    }

    /*
     * Found the node, but check if it came from an external table.
     * Add it to external list. Note: Node->OwnerId == 0 indicates
     * one of the built-in ACPI Names (_OS_ etc.) which can safely
     * be ignored.
     */
    else if (Node->OwnerId &&
            (WalkState->OwnerId != Node->OwnerId))
    {
        ObjectType2 = ObjectType;

        Object = AcpiNsGetAttachedObject (Node);
        if (Object)
        {
            ObjectType2 = Object->Common.Type;
            if (ObjectType2 == ACPI_TYPE_METHOD)
            {
                ParamCount = Object->Method.ParamCount;
            }
        }

        Pathname = AcpiNsGetExternalPathname (Node);
        if (!Pathname)
        {
            return (AE_NO_MEMORY);
        }

        AcpiDmAddNodeToExternalList (Node, (UINT8) ObjectType2,
            ParamCount, ACPI_EXT_RESOLVED_REFERENCE);

        ACPI_FREE (Pathname);
        Op->Common.Node = Node;
    }
    else
    {
        Op->Common.Node = Node;
    }


Exit:
    /* Open new scope if necessary */

    if (AcpiNsOpensScope (ObjectType))
    {
        if (Op->Common.Node)
        {
            Status = AcpiDsScopeStackPush (Op->Common.Node, ObjectType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmResourceDescendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Process one parse op during symbolic resource index conversion.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmResourceDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_STATUS             Status;


    WalkState = Info->WalkState;
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    /* Open new scope if necessary */

    ObjectType = OpInfo->ObjectType;
    if (AcpiNsOpensScope (ObjectType))
    {
        if (Op->Common.Node)
        {

            Status = AcpiDsScopeStackPush (Op->Common.Node, ObjectType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    /*
     * Check if this operator contains a reference to a resource descriptor.
     * If so, convert the reference into a symbolic reference.
     */
    AcpiDmCheckResourceReference (Op, WalkState);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommonAscendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Ascending handler for combined parse/namespace walks. Closes
 *              scope if necessary.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmCommonAscendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_OBJECT_TYPE        ObjectType;


    /* Close scope if necessary */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    ObjectType = OpInfo->ObjectType;
    ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);

    if (AcpiNsOpensScope (ObjectType))
    {
        (void) AcpiDsScopeStackPop (Info->WalkState);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmInspectPossibleArgs
 *
 * PARAMETERS:  CurrentOpArgCount   - Which arg of the current op was the
 *                                    possible method invocation found
 *              TargetCount         - Number of targets (0,1,2) for this op
 *              Op                  - Parse op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Examine following args and next ops for possible arguments
 *              for an unrecognized method invocation.
 *
 ******************************************************************************/

static UINT32
AcpiDmInspectPossibleArgs (
    UINT32                  CurrentOpArgCount,
    UINT32                  TargetCount,
    ACPI_PARSE_OBJECT       *Op)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  i;
    UINT32                  Last = 0;
    UINT32                  Lookahead;


    Lookahead = (ACPI_METHOD_NUM_ARGS + TargetCount) - CurrentOpArgCount;

    /* Lookahead for the maximum number of possible arguments */

    for (i = 0; i < Lookahead; i++)
    {
        if (!Op)
        {
            break;
        }

        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

        /*
         * Any one of these operators is "very probably" not a method arg
         */
        if ((Op->Common.AmlOpcode == AML_STORE_OP) ||
            (Op->Common.AmlOpcode == AML_NOTIFY_OP))
        {
            break;
        }

        if ((OpInfo->Class != AML_CLASS_EXECUTE) &&
            (OpInfo->Class != AML_CLASS_CONTROL))
        {
            Last = i+1;
        }

        Op = Op->Common.Next;
    }

    return (Last);
}
