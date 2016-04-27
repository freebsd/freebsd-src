/******************************************************************************
 *
 * Module Name: dswload - Dispatcher namespace load callbacks
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/acnamesp.h>

#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslload")

/* Local prototypes */

static ACPI_STATUS
LdLoadFieldElements (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

static ACPI_STATUS
LdLoadResourceElements (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

static ACPI_STATUS
LdNamespace1Begin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
LdNamespace2Begin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
LdCommonNamespaceEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*******************************************************************************
 *
 * FUNCTION:    LdLoadNamespace
 *
 * PARAMETERS:  RootOp      - Root of the parse tree
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform a walk of the parse tree that in turn loads all of the
 *              named ASL/AML objects into the namespace. The namespace is
 *              constructed in order to resolve named references and references
 *              to named fields within resource templates/descriptors.
 *
 ******************************************************************************/

ACPI_STATUS
LdLoadNamespace (
    ACPI_PARSE_OBJECT       *RootOp)
{
    ACPI_WALK_STATE         *WalkState;


    /* Create a new walk state */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    if (!WalkState)
    {
        return (AE_NO_MEMORY);
    }

    /* Walk the entire parse tree, first pass */

    TrWalkParseTree (RootOp, ASL_WALK_VISIT_TWICE, LdNamespace1Begin,
        LdCommonNamespaceEnd, WalkState);

    /* Second pass to handle forward references */

    TrWalkParseTree (RootOp, ASL_WALK_VISIT_TWICE, LdNamespace2Begin,
        LdCommonNamespaceEnd, WalkState);

    /* Dump the namespace if debug is enabled */

    AcpiNsDumpTables (ACPI_NS_ALL, ACPI_UINT32_MAX);
    ACPI_FREE (WalkState);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LdLoadFieldElements
 *
 * PARAMETERS:  Op              - Parent node (Field)
 *              WalkState       - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter the named elements of the field (children of the parent)
 *              into the namespace.
 *
 ******************************************************************************/

static ACPI_STATUS
LdLoadFieldElements (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *Child = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Get the first named field element */

    switch (Op->Asl.AmlOpcode)
    {
    case AML_BANK_FIELD_OP:

        Child = UtGetArg (Op, 6);
        break;

    case AML_INDEX_FIELD_OP:

        Child = UtGetArg (Op, 5);
        break;

    case AML_FIELD_OP:

        Child = UtGetArg (Op, 4);
        break;

    default:

        /* No other opcodes should arrive here */

        return (AE_BAD_PARAMETER);
    }

    /* Enter all elements into the namespace */

    while (Child)
    {
        switch (Child->Asl.AmlOpcode)
        {
        case AML_INT_RESERVEDFIELD_OP:
        case AML_INT_ACCESSFIELD_OP:
        case AML_INT_CONNECTION_OP:
            break;

        default:

            Status = AcpiNsLookup (WalkState->ScopeInfo,
                Child->Asl.Value.String,
                ACPI_TYPE_LOCAL_REGION_FIELD,
                ACPI_IMODE_LOAD_PASS1,
                ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE |
                    ACPI_NS_ERROR_IF_FOUND, NULL, &Node);
            if (ACPI_FAILURE (Status))
            {
                if (Status != AE_ALREADY_EXISTS)
                {
                    AslError (ASL_ERROR, ASL_MSG_CORE_EXCEPTION, Child,
                        Child->Asl.Value.String);
                    return (Status);
                }

                /*
                 * The name already exists in this scope
                 * But continue processing the elements
                 */
                AslError (ASL_ERROR, ASL_MSG_NAME_EXISTS, Child,
                    Child->Asl.Value.String);
            }
            else
            {
                Child->Asl.Node = Node;
                Node->Op = Child;
            }
            break;
        }

        Child = Child->Asl.Next;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LdLoadResourceElements
 *
 * PARAMETERS:  Op              - Parent node (Resource Descriptor)
 *              WalkState       - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter the named elements of the resource descriptor (children
 *              of the parent) into the namespace.
 *
 * NOTE: In the real AML namespace, these named elements never exist. But
 *       we simply use the namespace here as a symbol table so we can look
 *       them up as they are referenced.
 *
 ******************************************************************************/

static ACPI_STATUS
LdLoadResourceElements (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *InitializerOp = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /*
     * Enter the resource name into the namespace. Name must not already exist.
     * This opens a scope, so later field names are guaranteed to be new/unique.
     */
    Status = AcpiNsLookup (WalkState->ScopeInfo, Op->Asl.Namepath,
        ACPI_TYPE_LOCAL_RESOURCE, ACPI_IMODE_LOAD_PASS1,
        ACPI_NS_NO_UPSEARCH | ACPI_NS_ERROR_IF_FOUND,
        WalkState, &Node);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_ALREADY_EXISTS)
        {
            /* Actual node causing the error was saved in ParentMethod */

            AslError (ASL_ERROR, ASL_MSG_NAME_EXISTS,
                (ACPI_PARSE_OBJECT *) Op->Asl.ParentMethod, Op->Asl.Namepath);
            return (AE_OK);
        }
        return (Status);
    }

    Node->Value = (UINT32) Op->Asl.Value.Integer;
    Node->Op = Op;
    Op->Asl.Node = Node;

    /*
     * Now enter the predefined fields, for easy lookup when referenced
     * by the source ASL
     */
    InitializerOp = ASL_GET_CHILD_NODE (Op);
    while (InitializerOp)
    {
        if (InitializerOp->Asl.ExternalName)
        {
            Status = AcpiNsLookup (WalkState->ScopeInfo,
                InitializerOp->Asl.ExternalName,
                ACPI_TYPE_LOCAL_RESOURCE_FIELD,
                ACPI_IMODE_LOAD_PASS1,
                ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE,
                NULL, &Node);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            /*
             * Store the field offset and length in the namespace node
             * so it can be used when the field is referenced
             */
            Node->Value = InitializerOp->Asl.Value.Tag.BitOffset;
            Node->Length = InitializerOp->Asl.Value.Tag.BitLength;
            InitializerOp->Asl.Node = Node;
            Node->Op = InitializerOp;
        }

        InitializerOp = ASL_GET_PEER_NODE (InitializerOp);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LdNamespace1Begin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the parse tree walk. If this
 *              is a named AML opcode, enter into the namespace
 *
 ******************************************************************************/

static ACPI_STATUS
LdNamespace1Begin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *MethodOp;
    ACPI_STATUS             Status;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_OBJECT_TYPE        ActualObjectType = ACPI_TYPE_ANY;
    char                    *Path;
    UINT32                  Flags = ACPI_NS_NO_UPSEARCH;
    ACPI_PARSE_OBJECT       *Arg;
    UINT32                  i;
    BOOLEAN                 ForceNewScope = FALSE;


    ACPI_FUNCTION_NAME (LdNamespace1Begin);
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Op %p [%s]\n",
        Op, Op->Asl.ParseOpName));

    /*
     * We are only interested in opcodes that have an associated name
     * (or multiple names)
     */
    switch (Op->Asl.AmlOpcode)
    {
    case AML_BANK_FIELD_OP:
    case AML_INDEX_FIELD_OP:
    case AML_FIELD_OP:

        Status = LdLoadFieldElements (Op, WalkState);
        return (Status);

    case AML_INT_CONNECTION_OP:


        if (Op->Asl.Child->Asl.AmlOpcode != AML_INT_NAMEPATH_OP)
        {
            break;
        }
        Arg = Op->Asl.Child;

        Status = AcpiNsLookup (WalkState->ScopeInfo, Arg->Asl.ExternalName,
            ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
            WalkState, &Node);
        if (ACPI_FAILURE (Status))
        {
            break;
        }

        if (Node->Type == ACPI_TYPE_BUFFER)
        {
            Arg->Asl.Node = Node;

            Arg = Node->Op->Asl.Child;  /* Get namepath */
            Arg = Arg->Asl.Next;        /* Get actual buffer */
            Arg = Arg->Asl.Child;       /* Buffer length */
            Arg = Arg->Asl.Next;        /* RAW_DATA buffer */
        }
        break;

    default:

        /* All other opcodes go below */

        break;
    }

    /* Check if this object has already been installed in the namespace */

    if (Op->Asl.Node)
    {
        return (AE_OK);
    }

    Path = Op->Asl.Namepath;
    if (!Path)
    {
        return (AE_OK);
    }

    /* Map the raw opcode into an internal object type */

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_NAME:

        Arg = Op->Asl.Child;  /* Get the NameSeg/NameString node */
        Arg = Arg->Asl.Next;  /* First peer is the object to be associated with the name */

        /*
         * If this name refers to a ResourceTemplate, we will need to open
         * a new scope so that the resource subfield names can be entered into
         * the namespace underneath this name
         */
        if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC)
        {
            ForceNewScope = TRUE;
        }

        /* Get the data type associated with the named object, not the name itself */

        /* Log2 loop to convert from Btype (binary) to Etype (encoded) */

        ObjectType = 1;
        for (i = 1; i < Arg->Asl.AcpiBtype; i *= 2)
        {
            ObjectType++;
        }
        break;


    case PARSEOP_EXTERNAL:
        /*
         * "External" simply enters a name and type into the namespace.
         * We must be careful to not open a new scope, however, no matter
         * what type the external name refers to (e.g., a method)
         *
         * first child is name, next child is ObjectType
         */
        ActualObjectType = (UINT8) Op->Asl.Child->Asl.Next->Asl.Value.Integer;
        ObjectType = ACPI_TYPE_ANY;

        /*
         * We will mark every new node along the path as "External". This
         * allows some or all of the nodes to be created later in the ASL
         * code. Handles cases like this:
         *
         *   External (\_SB_.PCI0.ABCD, IntObj)
         *   Scope (_SB_)
         *   {
         *       Device (PCI0)
         *       {
         *       }
         *   }
         *   Method (X)
         *   {
         *       Store (\_SB_.PCI0.ABCD, Local0)
         *   }
         */
        Flags |= ACPI_NS_EXTERNAL;
        break;

    case PARSEOP_DEFAULT_ARG:

        if (Op->Asl.CompileFlags == NODE_IS_RESOURCE_DESC)
        {
            Status = LdLoadResourceElements (Op, WalkState);
            return_ACPI_STATUS (Status);
        }

        ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
        break;

    case PARSEOP_SCOPE:
        /*
         * The name referenced by Scope(Name) must already exist at this point.
         * In other words, forward references for Scope() are not supported.
         * The only real reason for this is that the MS interpreter cannot
         * handle this case. Perhaps someday this case can go away.
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ACPI_TYPE_ANY,
            ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
            WalkState, &(Node));
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_NOT_FOUND)
            {
                /* The name was not found, go ahead and create it */

                Status = AcpiNsLookup (WalkState->ScopeInfo, Path,
                    ACPI_TYPE_LOCAL_SCOPE,
                    ACPI_IMODE_LOAD_PASS1, Flags,
                    WalkState, &(Node));
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }

                /*
                 * However, this is an error -- primarily because the MS
                 * interpreter can't handle a forward reference from the
                 * Scope() operator.
                 */
                AslError (ASL_ERROR, ASL_MSG_NOT_FOUND, Op,
                    Op->Asl.ExternalName);
                AslError (ASL_ERROR, ASL_MSG_SCOPE_FWD_REF, Op,
                    Op->Asl.ExternalName);
                goto FinishNode;
            }

            AslCoreSubsystemError (Op, Status,
                "Failure from namespace lookup", FALSE);

            return_ACPI_STATUS (Status);
        }
        else /* Status AE_OK */
        {
            /*
             * Do not allow references to external scopes from the DSDT.
             * This is because the DSDT is always loaded first, and the
             * external reference cannot be resolved -- causing a runtime
             * error because Scope() must be resolved immediately.
             * 10/2015.
             */
            if ((Node->Flags & ANOBJ_IS_EXTERNAL) &&
                (ACPI_COMPARE_NAME (Gbl_TableSignature, "DSDT")))
            {
                /* However, allowed if the reference is within a method */

                MethodOp = Op->Asl.Parent;
                while (MethodOp &&
                      (MethodOp->Asl.ParseOpcode != PARSEOP_METHOD))
                {
                    MethodOp = MethodOp->Asl.Parent;
                }

                if (!MethodOp)
                {
                    /* Not in a control method, error */

                    AslError (ASL_ERROR, ASL_MSG_CROSS_TABLE_SCOPE, Op, NULL);
                }
            }
        }

        /* We found a node with this name, now check the type */

        switch (Node->Type)
        {
        case ACPI_TYPE_LOCAL_SCOPE:
        case ACPI_TYPE_DEVICE:
        case ACPI_TYPE_POWER:
        case ACPI_TYPE_PROCESSOR:
        case ACPI_TYPE_THERMAL:

            /* These are acceptable types - they all open a new scope */
            break;

        case ACPI_TYPE_INTEGER:
        case ACPI_TYPE_STRING:
        case ACPI_TYPE_BUFFER:
            /*
             * These types we will allow, but we will change the type.
             * This enables some existing code of the form:
             *
             *  Name (DEB, 0)
             *  Scope (DEB) { ... }
             *
             * Which is used to workaround the fact that the MS interpreter
             * does not allow Scope() forward references.
             */
            sprintf (MsgBuffer, "%s [%s], changing type to [Scope]",
                Op->Asl.ExternalName, AcpiUtGetTypeName (Node->Type));
            AslError (ASL_REMARK, ASL_MSG_SCOPE_TYPE, Op, MsgBuffer);

            /* Switch the type to scope, open the new scope */

            Node->Type = ACPI_TYPE_LOCAL_SCOPE;
            Status = AcpiDsScopeStackPush (Node, ACPI_TYPE_LOCAL_SCOPE,
                WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;

        default:

            /* All other types are an error */

            sprintf (MsgBuffer, "%s [%s]", Op->Asl.ExternalName,
                AcpiUtGetTypeName (Node->Type));
            AslError (ASL_ERROR, ASL_MSG_SCOPE_TYPE, Op, MsgBuffer);

            /*
             * However, switch the type to be an actual scope so
             * that compilation can continue without generating a whole
             * cascade of additional errors. Open the new scope.
             */
            Node->Type = ACPI_TYPE_LOCAL_SCOPE;
            Status = AcpiDsScopeStackPush (Node, ACPI_TYPE_LOCAL_SCOPE,
                WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;
        }

        Status = AE_OK;
        goto FinishNode;


    default:

        ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
        break;
    }


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Loading name: %s, (%s)\n",
        Op->Asl.ExternalName, AcpiUtGetTypeName (ObjectType)));

    /* The name must not already exist */

    Flags |= ACPI_NS_ERROR_IF_FOUND;

    /*
     * Enter the named type into the internal namespace. We enter the name
     * as we go downward in the parse tree. Any necessary subobjects that
     * involve arguments to the opcode must be created as we go back up the
     * parse tree later.
     */
    Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
        ACPI_IMODE_LOAD_PASS1, Flags, WalkState, &Node);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_ALREADY_EXISTS)
        {
            /* The name already exists in this scope */

            if (Node->Type == ACPI_TYPE_LOCAL_SCOPE)
            {
                /* Allow multiple references to the same scope */

                Node->Type = (UINT8) ObjectType;
                Status = AE_OK;
            }
            else if ((Node->Flags & ANOBJ_IS_EXTERNAL) &&
                     (Op->Asl.ParseOpcode != PARSEOP_EXTERNAL))
            {
                /*
                 * Allow one create on an object or segment that was
                 * previously declared External
                 */
                Node->Flags &= ~ANOBJ_IS_EXTERNAL;
                Node->Type = (UINT8) ObjectType;

                /* Just retyped a node, probably will need to open a scope */

                if (AcpiNsOpensScope (ObjectType))
                {
                    Status = AcpiDsScopeStackPush (Node, ObjectType, WalkState);
                    if (ACPI_FAILURE (Status))
                    {
                        return_ACPI_STATUS (Status);
                    }
                }

                Status = AE_OK;
            }
            else if (!(Node->Flags & ANOBJ_IS_EXTERNAL) &&
                     (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL))
            {
                /*
                 * Allow externals in same scope as the definition of the
                 * actual object. Similar to C. Allows multiple definition
                 * blocks that refer to each other in the same file.
                 */
                Status = AE_OK;
            }
            else if ((Node->Flags & ANOBJ_IS_EXTERNAL) &&
                     (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL) &&
                     (ObjectType == ACPI_TYPE_ANY))
            {
                /* Allow update of externals of unknown type. */

                if (AcpiNsOpensScope (ActualObjectType))
                {
                    Node->Type = (UINT8) ActualObjectType;
                    Status = AE_OK;
                }
                else
                {
                    sprintf (MsgBuffer, "%s [%s]", Op->Asl.ExternalName,
                        AcpiUtGetTypeName (Node->Type));
                    AslError (ASL_ERROR, ASL_MSG_SCOPE_TYPE, Op, MsgBuffer);
                    return_ACPI_STATUS (AE_OK);
                }
            }
            else
            {
                /* Valid error, object already exists */

                AslError (ASL_ERROR, ASL_MSG_NAME_EXISTS, Op,
                    Op->Asl.ExternalName);
                return_ACPI_STATUS (AE_OK);
            }
        }
        else
        {
            AslCoreSubsystemError (Op, Status,
                "Failure from namespace lookup", FALSE);
            return_ACPI_STATUS (Status);
        }
    }

    if (ForceNewScope)
    {
        Status = AcpiDsScopeStackPush (Node, ObjectType, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

FinishNode:
    /*
     * Point the parse node to the new namespace node, and point
     * the Node back to the original Parse node
     */
    Op->Asl.Node = Node;
    Node->Op = Op;

    /* Set the actual data type if appropriate (EXTERNAL term only) */

    if (ActualObjectType != ACPI_TYPE_ANY)
    {
        Node->Type = (UINT8) ActualObjectType;
        Node->Value = ASL_EXTERNAL_METHOD;
    }

    if (Op->Asl.ParseOpcode == PARSEOP_METHOD)
    {
        /*
         * Get the method argument count from "Extra" and save
         * it in the namespace node
         */
        Node->Value = (UINT32) Op->Asl.Extra;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    LdNamespace2Begin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the pass 2 parse tree walk.
 *              Second pass resolves some forward references.
 *
 * Notes:
 * Currently only needs to handle the Alias operator.
 * Could be used to allow forward references from the Scope() operator, but
 * the MS interpreter does not allow this, so this compiler does not either.
 *
 ******************************************************************************/

static ACPI_STATUS
LdNamespace2Begin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OBJECT_TYPE        ObjectType;
    BOOLEAN                 ForceNewScope = FALSE;
    ACPI_PARSE_OBJECT       *Arg;
    char                    *Path;
    ACPI_NAMESPACE_NODE     *TargetNode;


    ACPI_FUNCTION_NAME (LdNamespace2Begin);
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Op %p [%s]\n",
        Op, Op->Asl.ParseOpName));


    /* Ignore Ops with no namespace node */

    Node = Op->Asl.Node;
    if (!Node)
    {
        return (AE_OK);
    }

    /* Get the type to determine if we should push the scope */

    if ((Op->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG) &&
        (Op->Asl.CompileFlags == NODE_IS_RESOURCE_DESC))
    {
        ObjectType = ACPI_TYPE_LOCAL_RESOURCE;
    }
    else
    {
        ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
    }

    /* Push scope for Resource Templates */

    if (Op->Asl.ParseOpcode == PARSEOP_NAME)
    {
        if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC)
        {
            ForceNewScope = TRUE;
        }
    }

    /* Push the scope stack */

    if (ForceNewScope || AcpiNsOpensScope (ObjectType))
    {
        Status = AcpiDsScopeStackPush (Node, ObjectType, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    if (Op->Asl.ParseOpcode == PARSEOP_ALIAS)
    {
        /* Complete the alias node by getting and saving the target node */

        /* First child is the alias target */

        Arg = Op->Asl.Child;

        /* Get the target pathname */

        Path = Arg->Asl.Namepath;
        if (!Path)
        {
            Status = UtInternalizeName (Arg->Asl.ExternalName, &Path);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }

        /* Get the NS node associated with the target. It must exist. */

        Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ACPI_TYPE_ANY,
            ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
            WalkState, &TargetNode);
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_NOT_FOUND)
            {
                AslError (ASL_ERROR, ASL_MSG_NOT_FOUND, Op,
                    Op->Asl.ExternalName);

                /*
                 * The name was not found, go ahead and create it.
                 * This prevents more errors later.
                 */
                Status = AcpiNsLookup (WalkState->ScopeInfo, Path,
                    ACPI_TYPE_ANY,
                    ACPI_IMODE_LOAD_PASS1, ACPI_NS_NO_UPSEARCH,
                    WalkState, &(Node));
                return (AE_OK);
            }

            AslCoreSubsystemError (Op, Status,
                "Failure from namespace lookup", FALSE);
            return (AE_OK);
        }

        /* Save the target node within the alias node */

        Node->Object = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, TargetNode);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LdCommonNamespaceEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during the loading of the namespace,
 *              We only need to worry about managing the scope stack here.
 *
 ******************************************************************************/

static ACPI_STATUS
LdCommonNamespaceEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    ACPI_OBJECT_TYPE        ObjectType;
    BOOLEAN                 ForceNewScope = FALSE;


    ACPI_FUNCTION_NAME (LdCommonNamespaceEnd);


    /* We are only interested in opcodes that have an associated name */

    if (!Op->Asl.Namepath)
    {
        return (AE_OK);
    }

    /* Get the type to determine if we should pop the scope */

    if ((Op->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG) &&
        (Op->Asl.CompileFlags == NODE_IS_RESOURCE_DESC))
    {
        /* TBD: Merge into AcpiDsMapNamedOpcodeToDataType */

        ObjectType = ACPI_TYPE_LOCAL_RESOURCE;
    }
    else
    {
        ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
    }

    /* Pop scope that was pushed for Resource Templates */

    if (Op->Asl.ParseOpcode == PARSEOP_NAME)
    {
        if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC)
        {
            ForceNewScope = TRUE;
        }
    }

    /* Pop the scope stack */

    if (ForceNewScope || AcpiNsOpensScope (ObjectType))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "(%s): Popping scope for Op [%s] %p\n",
            AcpiUtGetTypeName (ObjectType), Op->Asl.ParseOpName, Op));

        (void) AcpiDsScopeStackPop (WalkState);
    }

    return (AE_OK);
}
