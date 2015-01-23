/******************************************************************************
 *
 * Module Name: aslxref - Namespace cross-reference
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdispat.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslxref")

/* Local prototypes */

static ACPI_STATUS
XfNamespaceLocateBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
XfNamespaceLocateEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static BOOLEAN
XfObjectExists (
    char                    *Name);

static ACPI_STATUS
XfCompareOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static void
XfCheckFieldRange (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  RegionBitLength,
    UINT32                  FieldBitOffset,
    UINT32                  FieldBitLength,
    UINT32                  AccessBitWidth);

#ifdef __UNDER_DEVELOPMENT
static ACPI_PARSE_OBJECT *
XfGetParentMethod (
    ACPI_PARSE_OBJECT       *Op);

static void
XfCheckIllegalReference (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *Node);

static BOOLEAN
XfIsObjectParental (
    ACPI_PARSE_OBJECT       *MethodOp1,
    ACPI_PARSE_OBJECT       *MethodOp2);
#endif


/*******************************************************************************
 *
 * FUNCTION:    XfCrossReferenceNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform a cross reference check of the parse tree against the
 *              namespace. Every named referenced within the parse tree
 *              should be get resolved with a namespace lookup. If not, the
 *              original reference in the ASL code is invalid -- i.e., refers
 *              to a non-existent object.
 *
 * NOTE:  The ASL "External" operator causes the name to be inserted into the
 *        namespace so that references to the external name will be resolved
 *        correctly here.
 *
 ******************************************************************************/

ACPI_STATUS
XfCrossReferenceNamespace (
    void)
{
    ACPI_WALK_STATE         *WalkState;


    DbgPrint (ASL_DEBUG_OUTPUT, "\nCross referencing namespace\n\n");

    /*
     * Create a new walk state for use when looking up names
     * within the namespace (Passed as context to the callbacks)
     */
    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    if (!WalkState)
    {
        return (AE_NO_MEMORY);
    }

    /* Walk the entire parse tree */

    TrWalkParseTree (RootNode, ASL_WALK_VISIT_TWICE, XfNamespaceLocateBegin,
                        XfNamespaceLocateEnd, WalkState);

    ACPI_FREE (WalkState);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    XfObjectExists
 *
 * PARAMETERS:  Name            - 4 char ACPI name
 *
 * RETURN:      TRUE if name exists in namespace
 *
 * DESCRIPTION: Walk the namespace to find an object
 *
 ******************************************************************************/

static BOOLEAN
XfObjectExists (
    char                    *Name)
{
    ACPI_STATUS             Status;


    /* Walk entire namespace from the supplied root */

    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                ACPI_UINT32_MAX, FALSE, XfCompareOneNamespaceObject, NULL,
                Name, NULL);
    if (Status == AE_CTRL_TRUE)
    {
        /* At least one instance of the name was found */

        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    XfCompareOneNamespaceObject
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compare name of one object.
 *
 ******************************************************************************/

static ACPI_STATUS
XfCompareOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;


    /* Simply check the name */

    if (*((UINT32 *) (Context)) == Node->Name.Integer)
    {
        /* Abort walk if we found one instance */

        return (AE_CTRL_TRUE);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    XfCheckFieldRange
 *
 * PARAMETERS:  RegionBitLength     - Length of entire parent region
 *              FieldBitOffset      - Start of the field unit (within region)
 *              FieldBitLength      - Entire length of field unit
 *              AccessBitWidth      - Access width of the field unit
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check one field unit to make sure it fits in the parent
 *              op region.
 *
 * Note: AccessBitWidth must be either 8,16,32, or 64
 *
 ******************************************************************************/

static void
XfCheckFieldRange (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  RegionBitLength,
    UINT32                  FieldBitOffset,
    UINT32                  FieldBitLength,
    UINT32                  AccessBitWidth)
{
    UINT32                  FieldEndBitOffset;


    /*
     * Check each field unit against the region size. The entire
     * field unit (start offset plus length) must fit within the
     * region.
     */
    FieldEndBitOffset = FieldBitOffset + FieldBitLength;

    if (FieldEndBitOffset > RegionBitLength)
    {
        /* Field definition itself is beyond the end-of-region */

        AslError (ASL_ERROR, ASL_MSG_FIELD_UNIT_OFFSET, Op, NULL);
        return;
    }

    /*
     * Now check that the field plus AccessWidth doesn't go beyond
     * the end-of-region. Assumes AccessBitWidth is a power of 2
     */
    FieldEndBitOffset = ACPI_ROUND_UP (FieldEndBitOffset, AccessBitWidth);

    if (FieldEndBitOffset > RegionBitLength)
    {
        /* Field definition combined with the access is beyond EOR */

        AslError (ASL_ERROR, ASL_MSG_FIELD_UNIT_ACCESS_WIDTH, Op, NULL);
    }
}


#ifdef __UNDER_DEVELOPMENT
/*******************************************************************************
 *
 * FUNCTION:    XfIsObjectParental
 *
 * PARAMETERS:  ChildOp                 - Op to be checked
 *              PossibleParentOp        - Determine if this op is in the family
 *
 * RETURN:      TRUE if ChildOp is a descendent of PossibleParentOp
 *
 * DESCRIPTION: Determine if an Op is a descendent of another Op. Used to
 *              detect if a method is declared within another method.
 *
 ******************************************************************************/

static BOOLEAN
XfIsObjectParental (
    ACPI_PARSE_OBJECT       *ChildOp,
    ACPI_PARSE_OBJECT       *PossibleParentOp)
{
    ACPI_PARSE_OBJECT       *ParentOp;


    /* Search upwards through the tree for possible parent */

    ParentOp = ChildOp;
    while (ParentOp)
    {
        if (ParentOp == PossibleParentOp)
        {
            return (TRUE);
        }

        ParentOp = ParentOp->Asl.Parent;
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    XfGetParentMethod
 *
 * PARAMETERS:  Op                      - Op to be checked
 *
 * RETURN:      Op for parent method. NULL if object is not within a method.
 *
 * DESCRIPTION: Determine if an object is within a control method. Used to
 *              implement special rules for named references from within a
 *              control method.
 *
 * NOTE: It would be better to have the parser set a flag in the Op if possible.
 *
 ******************************************************************************/

static ACPI_PARSE_OBJECT *
XfGetParentMethod (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *ParentOp;


    if (!Op)
    {
        return (NULL);
    }

    if (Op->Asl.ParseOpcode == PARSEOP_METHOD)
    {
        return (NULL);
    }

    /* Walk upwards through the parse tree, up to the root if necessary */

    ParentOp = Op;
    while (ParentOp)
    {
        if (ParentOp->Asl.ParseOpcode == PARSEOP_METHOD)
        {
            return (ParentOp);
        }

        ParentOp = ParentOp->Asl.Parent;
    }

    /* Object is not within a method */

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    XfCheckIllegalReference
 *
 * PARAMETERS:  Op                      - Op referring to the target
 *              TargetNode              - Target of the reference
 *
 * RETURN:      None. Emits error message for an illegal reference
 *
 * DESCRIPTION: Determine if a named reference is legal. A "named" reference
 *              is something like: Store(ABCD, ...), where ABCD is an AML
 *              Nameseg or Namepath.
 *
 * NOTE: Caller must ensure that the name Op is in fact a reference, and not
 *       an actual name declaration (creation of a named object).
 *
 ******************************************************************************/

static void
XfCheckIllegalReference (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *TargetNode)
{
    ACPI_PARSE_OBJECT       *MethodOp1;
    ACPI_PARSE_OBJECT       *MethodOp2;
    ACPI_PARSE_OBJECT       *TargetOp;


    /*
     * Check for an illegal reference to a named object:
     *
     * 1) References from one control method to another, non-parent
     *    method are not allowed, they will fail at runtime.
     *
     * 2) Forward references within a control method are not allowed.
     *    AML interpreters use a one-pass parse of control methods
     *    so these forward references will fail at runtime.
     */
    TargetOp = TargetNode->Op;

    MethodOp1 = XfGetParentMethod (Op);
    MethodOp2 = XfGetParentMethod (TargetOp);

    /* Are both objects within control method(s)? */

    if (!MethodOp1 || !MethodOp2)
    {
        return;
    }

    /* Objects not in the same method? */

    if (MethodOp1 != MethodOp2)
    {
        /*
         * 1) Cross-method named reference
         *
         * This is OK if and only if the target reference is within in a
         * method that is a parent of current method
         */
        if (!XfIsObjectParental (MethodOp1, MethodOp2))
        {
            AslError (ASL_ERROR, ASL_MSG_ILLEGAL_METHOD_REF, Op,
                Op->Asl.ExternalName);
        }
    }

    /*
     * 2) Both reference and target are in the same method. Check if this is
     * an (illegal) forward reference by examining the exact source code
     * location of each (the referenced object and the object declaration).
     * This is a bit nasty, yet effective.
     */
    else if (Op->Asl.LogicalByteOffset < TargetOp->Asl.LogicalByteOffset)
    {
        AslError (ASL_ERROR, ASL_MSG_ILLEGAL_FORWARD_REF, Op,
            Op->Asl.ExternalName);
    }

}
#endif


/*******************************************************************************
 *
 * FUNCTION:    XfNamespaceLocateBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during cross-reference. For named
 *              object references, attempt to locate the name in the
 *              namespace.
 *
 * NOTE: ASL references to named fields within resource descriptors are
 *       resolved to integer values here. Therefore, this step is an
 *       important part of the code generation. We don't know that the
 *       name refers to a resource descriptor until now.
 *
 ******************************************************************************/

static ACPI_STATUS
XfNamespaceLocateBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    ACPI_OBJECT_TYPE        ObjectType;
    char                    *Path;
    UINT8                   PassedArgs;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_PARSE_OBJECT       *OwningOp;
    ACPI_PARSE_OBJECT       *SpaceIdOp;
    UINT32                  MinimumLength;
    UINT32                  Offset;
    UINT32                  FieldBitLength;
    UINT32                  TagBitLength;
    UINT8                   Message = 0;
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  Flags;


    ACPI_FUNCTION_TRACE_PTR (XfNamespaceLocateBegin, Op);

    /*
     * If this node is the actual declaration of a name
     * [such as the XXXX name in "Method (XXXX)"],
     * we are not interested in it here. We only care about names that are
     * references to other objects within the namespace and the parent objects
     * of name declarations
     */
    if (Op->Asl.CompileFlags & NODE_IS_NAME_DECLARATION)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* We are only interested in opcodes that have an associated name */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);

    if ((!(OpInfo->Flags & AML_NAMED)) &&
        (!(OpInfo->Flags & AML_CREATE)) &&
        (Op->Asl.ParseOpcode != PARSEOP_NAMESTRING) &&
        (Op->Asl.ParseOpcode != PARSEOP_NAMESEG)    &&
        (Op->Asl.ParseOpcode != PARSEOP_METHODCALL))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * One special case: CondRefOf operator - we don't care if the name exists
     * or not at this point, just ignore it, the point of the operator is to
     * determine if the name exists at runtime.
     */
    if ((Op->Asl.Parent) &&
        (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_CONDREFOF))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * We must enable the "search-to-root" for single NameSegs, but
     * we have to be very careful about opening up scopes
     */
    Flags = ACPI_NS_SEARCH_PARENT;
    if ((Op->Asl.ParseOpcode == PARSEOP_NAMESTRING) ||
        (Op->Asl.ParseOpcode == PARSEOP_NAMESEG)    ||
        (Op->Asl.ParseOpcode == PARSEOP_METHODCALL))
    {
        /*
         * These are name references, do not push the scope stack
         * for them.
         */
        Flags |= ACPI_NS_DONT_OPEN_SCOPE;
    }

    /* Get the NamePath from the appropriate place */

    if (OpInfo->Flags & AML_NAMED)
    {
        /* For nearly all NAMED operators, the name reference is the first child */

        Path = Op->Asl.Child->Asl.Value.String;
        if (Op->Asl.AmlOpcode == AML_ALIAS_OP)
        {
            /*
             * ALIAS is the only oddball opcode, the name declaration
             * (alias name) is the second operand
             */
            Path = Op->Asl.Child->Asl.Next->Asl.Value.String;
        }
    }
    else if (OpInfo->Flags & AML_CREATE)
    {
        /* Name must appear as the last parameter */

        NextOp = Op->Asl.Child;
        while (!(NextOp->Asl.CompileFlags & NODE_IS_NAME_DECLARATION))
        {
            NextOp = NextOp->Asl.Next;
        }
        Path = NextOp->Asl.Value.String;
    }
    else
    {
        Path = Op->Asl.Value.String;
    }

    ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "Type=%s\n", AcpiUtGetTypeName (ObjectType)));

    /*
     * Lookup the name in the namespace. Name must exist at this point, or it
     * is an invalid reference.
     *
     * The namespace is also used as a lookup table for references to resource
     * descriptors and the fields within them.
     */
    Gbl_NsLookupCount++;

    Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
                ACPI_IMODE_EXECUTE, Flags, WalkState, &(Node));
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_FOUND)
        {
            /*
             * We didn't find the name reference by path -- we can qualify this
             * a little better before we print an error message
             */
            if (strlen (Path) == ACPI_NAME_SIZE)
            {
                /* A simple, one-segment ACPI name */

                if (XfObjectExists (Path))
                {
                    /*
                     * There exists such a name, but we couldn't get to it
                     * from this scope
                     */
                    AslError (ASL_ERROR, ASL_MSG_NOT_REACHABLE, Op,
                        Op->Asl.ExternalName);
                }
                else
                {
                    /* The name doesn't exist, period */

                    AslError (ASL_ERROR, ASL_MSG_NOT_EXIST,
                        Op, Op->Asl.ExternalName);
                }
            }
            else
            {
                /* Check for a fully qualified path */

                if (Path[0] == AML_ROOT_PREFIX)
                {
                    /* Gave full path, the object does not exist */

                    AslError (ASL_ERROR, ASL_MSG_NOT_EXIST, Op,
                        Op->Asl.ExternalName);
                }
                else
                {
                    /*
                     * We can't tell whether it doesn't exist or just
                     * can't be reached.
                     */
                    AslError (ASL_ERROR, ASL_MSG_NOT_FOUND, Op,
                        Op->Asl.ExternalName);
                }
            }

            Status = AE_OK;
        }

        return_ACPI_STATUS (Status);
    }

    /* Check for a reference vs. name declaration */

    if (!(OpInfo->Flags & AML_NAMED) &&
        !(OpInfo->Flags & AML_CREATE))
    {
        /* This node has been referenced, mark it for reference check */

        Node->Flags |= ANOBJ_IS_REFERENCED;

#ifdef __UNDER_DEVELOPMENT

        /* Check for an illegal reference */

        XfCheckIllegalReference (Op, Node);
#endif
    }

    /* Attempt to optimize the NamePath */

    OptOptimizeNamePath (Op, OpInfo->Flags, WalkState, Path, Node);

    /*
     * 1) Dereference an alias (A name reference that is an alias)
     *    Aliases are not nested, the alias always points to the final object
     */
    if ((Op->Asl.ParseOpcode != PARSEOP_ALIAS) &&
        (Node->Type == ACPI_TYPE_LOCAL_ALIAS))
    {
        /* This node points back to the original PARSEOP_ALIAS */

        NextOp = Node->Op;

        /* The first child is the alias target op */

        NextOp = NextOp->Asl.Child;

        /* That in turn points back to original target alias node */

        if (NextOp->Asl.Node)
        {
            Node = NextOp->Asl.Node;
        }

        /* Else - forward reference to alias, will be resolved later */
    }

    /* 2) Check for a reference to a resource descriptor */

    if ((Node->Type == ACPI_TYPE_LOCAL_RESOURCE_FIELD) ||
        (Node->Type == ACPI_TYPE_LOCAL_RESOURCE))
    {
        /*
         * This was a reference to a field within a resource descriptor.
         * Extract the associated field offset (either a bit or byte
         * offset depending on the field type) and change the named
         * reference into an integer for AML code generation
         */
        Offset = Node->Value;
        TagBitLength = Node->Length;

        /*
         * If a field is being created, generate the length (in bits) of
         * the field. Note: Opcodes other than CreateXxxField and Index
         * can come through here. For other opcodes, we just need to
         * convert the resource tag reference to an integer offset.
         */
        switch (Op->Asl.Parent->Asl.AmlOpcode)
        {
        case AML_CREATE_FIELD_OP: /* Variable "Length" field, in bits */
            /*
             * We know the length operand is an integer constant because
             * we know that it contains a reference to a resource
             * descriptor tag.
             */
            FieldBitLength = (UINT32) Op->Asl.Next->Asl.Value.Integer;
            break;

        case AML_CREATE_BIT_FIELD_OP:

            FieldBitLength = 1;
            break;

        case AML_CREATE_BYTE_FIELD_OP:
        case AML_INDEX_OP:

            FieldBitLength = 8;
            break;

        case AML_CREATE_WORD_FIELD_OP:

            FieldBitLength = 16;
            break;

        case AML_CREATE_DWORD_FIELD_OP:

            FieldBitLength = 32;
            break;

        case AML_CREATE_QWORD_FIELD_OP:

            FieldBitLength = 64;
            break;

        default:

            FieldBitLength = 0;
            break;
        }

        /* Check the field length against the length of the resource tag */

        if (FieldBitLength)
        {
            if (TagBitLength < FieldBitLength)
            {
                Message = ASL_MSG_TAG_SMALLER;
            }
            else if (TagBitLength > FieldBitLength)
            {
                Message = ASL_MSG_TAG_LARGER;
            }

            if (Message)
            {
                sprintf (MsgBuffer, "Size mismatch, Tag: %u bit%s, Field: %u bit%s",
                    TagBitLength, (TagBitLength > 1) ? "s" : "",
                    FieldBitLength, (FieldBitLength > 1) ? "s" : "");

                AslError (ASL_WARNING, Message, Op, MsgBuffer);
            }
        }

        /* Convert the BitOffset to a ByteOffset for certain opcodes */

        switch (Op->Asl.Parent->Asl.AmlOpcode)
        {
        case AML_CREATE_BYTE_FIELD_OP:
        case AML_CREATE_WORD_FIELD_OP:
        case AML_CREATE_DWORD_FIELD_OP:
        case AML_CREATE_QWORD_FIELD_OP:
        case AML_INDEX_OP:

            Offset = ACPI_DIV_8 (Offset);
            break;

        default:

            break;
        }

        /* Now convert this node to an integer whose value is the field offset */

        Op->Asl.AmlLength = 0;
        Op->Asl.ParseOpcode = PARSEOP_INTEGER;
        Op->Asl.Value.Integer = (UINT64) Offset;
        Op->Asl.CompileFlags |= NODE_IS_RESOURCE_FIELD;

        OpcGenerateAmlOpcode (Op);
    }

    /* 3) Check for a method invocation */

    else if ((((Op->Asl.ParseOpcode == PARSEOP_NAMESTRING) || (Op->Asl.ParseOpcode == PARSEOP_NAMESEG)) &&
                (Node->Type == ACPI_TYPE_METHOD) &&
                (Op->Asl.Parent) &&
                (Op->Asl.Parent->Asl.ParseOpcode != PARSEOP_METHOD))   ||

                (Op->Asl.ParseOpcode == PARSEOP_METHODCALL))
    {
        /*
         * A reference to a method within one of these opcodes is not an
         * invocation of the method, it is simply a reference to the method.
         */
        if ((Op->Asl.Parent) &&
           ((Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_REFOF)      ||
            (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_DEREFOF)    ||
            (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_OBJECTTYPE)))
        {
            return_ACPI_STATUS (AE_OK);
        }
        /*
         * There are two types of method invocation:
         * 1) Invocation with arguments -- the parser recognizes this
         *    as a METHODCALL.
         * 2) Invocation with no arguments --the parser cannot determine that
         *    this is a method invocation, therefore we have to figure it out
         *    here.
         */
        if (Node->Type != ACPI_TYPE_METHOD)
        {
            sprintf (MsgBuffer, "%s is a %s",
                    Op->Asl.ExternalName, AcpiUtGetTypeName (Node->Type));

            AslError (ASL_ERROR, ASL_MSG_NOT_METHOD, Op, MsgBuffer);
            return_ACPI_STATUS (AE_OK);
        }

        /* Save the method node in the caller's op */

        Op->Asl.Node = Node;
        if (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_CONDREFOF)
        {
            return_ACPI_STATUS (AE_OK);
        }

        /*
         * This is a method invocation, with or without arguments.
         * Count the number of arguments, each appears as a child
         * under the parent node
         */
        Op->Asl.ParseOpcode = PARSEOP_METHODCALL;
        UtSetParseOpName (Op);

        PassedArgs = 0;
        NextOp     = Op->Asl.Child;

        while (NextOp)
        {
            PassedArgs++;
            NextOp = NextOp->Asl.Next;
        }

        if (Node->Value != ASL_EXTERNAL_METHOD)
        {
            /*
             * Check the parsed arguments with the number expected by the
             * method declaration itself
             */
            if (PassedArgs != Node->Value)
            {
                sprintf (MsgBuffer, "%s requires %u", Op->Asl.ExternalName,
                            Node->Value);

                if (PassedArgs < Node->Value)
                {
                    AslError (ASL_ERROR, ASL_MSG_ARG_COUNT_LO, Op, MsgBuffer);
                }
                else
                {
                    AslError (ASL_ERROR, ASL_MSG_ARG_COUNT_HI, Op, MsgBuffer);
                }
            }
        }
    }

    /* 4) Check for an ASL Field definition */

    else if ((Op->Asl.Parent) &&
            ((Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_FIELD)     ||
             (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_BANKFIELD)))
    {
        /*
         * Offset checking for fields. If the parent operation region has a
         * constant length (known at compile time), we can check fields
         * defined in that region against the region length. This will catch
         * fields and field units that cannot possibly fit within the region.
         *
         * Note: Index fields do not directly reference an operation region,
         * thus they are not included in this check.
         */
        if (Op == Op->Asl.Parent->Asl.Child)
        {
            /*
             * This is the first child of the field node, which is
             * the name of the region. Get the parse node for the
             * region -- which contains the length of the region.
             */
            OwningOp = Node->Op;
            Op->Asl.Parent->Asl.ExtraValue =
                ACPI_MUL_8 ((UINT32) OwningOp->Asl.Value.Integer);

            /* Examine the field access width */

            switch ((UINT8) Op->Asl.Parent->Asl.Value.Integer)
            {
            case AML_FIELD_ACCESS_ANY:
            case AML_FIELD_ACCESS_BYTE:
            case AML_FIELD_ACCESS_BUFFER:
            default:

                MinimumLength = 1;
                break;

            case AML_FIELD_ACCESS_WORD:

                MinimumLength = 2;
                break;

            case AML_FIELD_ACCESS_DWORD:

                MinimumLength = 4;
                break;

            case AML_FIELD_ACCESS_QWORD:

                MinimumLength = 8;
                break;
            }

            /*
             * Is the region at least as big as the access width?
             * Note: DataTableRegions have 0 length
             */
            if (((UINT32) OwningOp->Asl.Value.Integer) &&
                ((UINT32) OwningOp->Asl.Value.Integer < MinimumLength))
            {
                AslError (ASL_ERROR, ASL_MSG_FIELD_ACCESS_WIDTH, Op, NULL);
            }

            /*
             * Check EC/CMOS/SMBUS fields to make sure that the correct
             * access type is used (BYTE for EC/CMOS, BUFFER for SMBUS)
             */
            SpaceIdOp = OwningOp->Asl.Child->Asl.Next;
            switch ((UINT32) SpaceIdOp->Asl.Value.Integer)
            {
            case ACPI_ADR_SPACE_EC:
            case ACPI_ADR_SPACE_CMOS:
            case ACPI_ADR_SPACE_GPIO:

                if ((UINT8) Op->Asl.Parent->Asl.Value.Integer != AML_FIELD_ACCESS_BYTE)
                {
                    AslError (ASL_ERROR, ASL_MSG_REGION_BYTE_ACCESS, Op, NULL);
                }
                break;

            case ACPI_ADR_SPACE_SMBUS:
            case ACPI_ADR_SPACE_IPMI:
            case ACPI_ADR_SPACE_GSBUS:

                if ((UINT8) Op->Asl.Parent->Asl.Value.Integer != AML_FIELD_ACCESS_BUFFER)
                {
                    AslError (ASL_ERROR, ASL_MSG_REGION_BUFFER_ACCESS, Op, NULL);
                }
                break;

            default:

                /* Nothing to do for other address spaces */

                break;
            }
        }
        else
        {
            /*
             * This is one element of the field list. Check to make sure
             * that it does not go beyond the end of the parent operation region.
             *
             * In the code below:
             *    Op->Asl.Parent->Asl.ExtraValue      - Region Length (bits)
             *    Op->Asl.ExtraValue                  - Field start offset (bits)
             *    Op->Asl.Child->Asl.Value.Integer32  - Field length (bits)
             *    Op->Asl.Child->Asl.ExtraValue       - Field access width (bits)
             */
            if (Op->Asl.Parent->Asl.ExtraValue && Op->Asl.Child)
            {
                XfCheckFieldRange (Op,
                            Op->Asl.Parent->Asl.ExtraValue,
                            Op->Asl.ExtraValue,
                            (UINT32) Op->Asl.Child->Asl.Value.Integer,
                            Op->Asl.Child->Asl.ExtraValue);
            }
        }
    }

    /* 5) Check for a connection object */
#if 0
    else if (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_CONNECTION)
    {
        return_ACPI_STATUS (Status);
    }
#endif

    Op->Asl.Node = Node;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    XfNamespaceLocateEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during cross reference. We only
 *              need to worry about scope management here.
 *
 ******************************************************************************/

static ACPI_STATUS
XfNamespaceLocateEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    const ACPI_OPCODE_INFO  *OpInfo;


    ACPI_FUNCTION_TRACE (XfNamespaceLocateEnd);


    /* We are only interested in opcodes that have an associated name */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    if (!(OpInfo->Flags & AML_NAMED))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Not interested in name references, we did not open a scope for them */

    if ((Op->Asl.ParseOpcode == PARSEOP_NAMESTRING) ||
        (Op->Asl.ParseOpcode == PARSEOP_NAMESEG)    ||
        (Op->Asl.ParseOpcode == PARSEOP_METHODCALL))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Pop the scope stack if necessary */

    if (AcpiNsOpensScope (AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode)))
    {

        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "%s: Popping scope for Op %p\n",
            AcpiUtGetTypeName (OpInfo->ObjectType), Op));

        (void) AcpiDsScopeStackPop (WalkState);
    }

    return_ACPI_STATUS (AE_OK);
}
