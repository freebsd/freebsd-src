/******************************************************************************
 *
 * Module Name: aslload - compiler namespace load callbacks
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acparser.h>
#include "aslcompiler.y.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslload")

/* Local prototypes */

static ACPI_STATUS
LdLoadFieldElements (
    UINT32                  AmlType,
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

static void
LdCheckSpecialNames (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_PARSE_OBJECT       *Op);

static ACPI_STATUS
LdAnalyzeExternals (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OBJECT_TYPE        ExternalOpType,
    ACPI_WALK_STATE         *WalkState);


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

    if (AcpiDbgLevel & ACPI_LV_TABLES)
    {
        AcpiNsDumpTables (ACPI_NS_ALL, ACPI_UINT32_MAX);
    }

    ACPI_FREE (WalkState);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LdLoadFieldElements
 *
 * PARAMETERS:  AmlType         - Type to search
 *              Op              - Parent node (Field)
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
    UINT32                  AmlType,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *Child = NULL;
    ACPI_PARSE_OBJECT       *SourceRegion;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    char                    *ExternalPath;


    SourceRegion = UtGetArg (Op, 0);
    if (SourceRegion)
    {
        Status = AcpiNsLookup (WalkState->ScopeInfo,
            SourceRegion->Asl.Value.String, AmlType, ACPI_IMODE_EXECUTE,
            ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE, NULL, &Node);
        if (Status == AE_NOT_FOUND)
        {
            /*
             * If the named object is not found, it means that it is either a
             * forward reference or the named object does not exist.
             */
            SourceRegion->Asl.CompileFlags |= OP_NOT_FOUND_DURING_LOAD;
        }
    }

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
                else if (Status == AE_ALREADY_EXISTS &&
                    (Node->Flags & ANOBJ_IS_EXTERNAL))
                {
                    Node->Type = (UINT8) ACPI_TYPE_LOCAL_REGION_FIELD;
                    Node->Flags &= ~ANOBJ_IS_EXTERNAL;
                }
                else
                {
                    /*
                     * The name already exists in this scope
                     * But continue processing the elements
                     */
                    ExternalPath = AcpiNsGetNormalizedPathname (Node, TRUE);

                    AslDualParseOpError (ASL_ERROR, ASL_MSG_NAME_EXISTS, Child,
                        ExternalPath, ASL_MSG_FOUND_HERE, Node->Op,
                        ExternalPath);

                    if (ExternalPath)
                    {
                        ACPI_FREE (ExternalPath);
                    }
                }
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
    char                    *ExternalPath;


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

            ExternalPath = AcpiNsGetNormalizedPathname (Node, TRUE);

            AslDualParseOpError (ASL_ERROR, ASL_MSG_NAME_EXISTS,
                (ACPI_PARSE_OBJECT *) Op->Asl.ParentMethod,
                ExternalPath, ASL_MSG_FOUND_HERE, Node->Op,
                ExternalPath);

            if (ExternalPath)
            {
                ACPI_FREE (ExternalPath);
            }
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
                ACPI_TYPE_LOCAL_RESOURCE_FIELD, ACPI_IMODE_LOAD_PASS1,
                ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE, NULL, &Node);
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
    char                    *Path;
    UINT32                  Flags = ACPI_NS_NO_UPSEARCH;
    ACPI_PARSE_OBJECT       *Arg;
    UINT32                  i;
    BOOLEAN                 ForceNewScope = FALSE;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_PARSE_OBJECT       *ParentOp;
    char                    *ExternalPath;


    ACPI_FUNCTION_NAME (LdNamespace1Begin);


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Op %p [%s]\n",
        Op, Op->Asl.ParseOpName));

    /*
     * We are only interested in opcodes that have an associated name
     * (or multiple names)
     */
    switch (Op->Asl.AmlOpcode)
    {
    case AML_INDEX_FIELD_OP:

        Status = LdLoadFieldElements (ACPI_TYPE_LOCAL_REGION_FIELD, Op, WalkState);
        return (Status);

    case AML_BANK_FIELD_OP:
    case AML_FIELD_OP:

        Status = LdLoadFieldElements (ACPI_TYPE_REGION, Op, WalkState);
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

    /* Check for a possible illegal forward reference */

    if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG) ||
        (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING) ||
        (Op->Asl.ParseOpcode == PARSEOP_METHODCALL))
    {
        /*
         * Op->Asl.Namepath will be NULL for these opcodes.
         * These opcodes are guaranteed to have a parent.
         * Examine the parent opcode.
         */
        ParentOp = Op->Asl.Parent;
        OpInfo = AcpiPsGetOpcodeInfo (ParentOp->Asl.AmlOpcode);

        /*
         * Exclude all operators that actually declare a new name:
         *      Name (ABCD, 1) -> Ignore (AML_CLASS_NAMED_OBJECT)
         * We only want references to named objects:
         *      Store (2, WXYZ) -> Attempt to resolve the name
         */
        if ((Op->Asl.ParseOpcode != PARSEOP_METHODCALL) &&
            (OpInfo->Class == AML_CLASS_NAMED_OBJECT))
        {
            return (AE_OK);
        }

        /*
         * Check if the referenced object exists at this point during
         * the load:
         * 1) If it exists, then this cannot be a forward reference.
         * 2) If it does not exist, it could be a forward reference or
         * it truly does not exist (and no external declaration).
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo,
            Op->Asl.Value.Name, ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
            ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
            WalkState, &Node);
        if (Status == AE_NOT_FOUND)
        {
            /*
             * This is either a forward reference or the object truly
             * does not exist. The two cases can only be differentiated
             * during the cross-reference stage later. Mark the Op/Name
             * as not-found for now to indicate the need for further
             * processing.
             *
             * Special case: Allow forward references from elements of
             * Package objects. This provides compatibility with other
             * ACPI implementations. To correctly implement this, the
             * ACPICA table load defers package resolution until the entire
             * namespace has been loaded.
             */
            if ((ParentOp->Asl.ParseOpcode != PARSEOP_PACKAGE) &&
                (ParentOp->Asl.ParseOpcode != PARSEOP_VAR_PACKAGE))
            {
                Op->Asl.CompileFlags |= OP_NOT_FOUND_DURING_LOAD;
            }

            return (AE_OK);
        }

        return (Status);
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
        if (Op->Asl.CompileFlags & OP_IS_RESOURCE_DESC)
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
        ObjectType = (UINT8) Op->Asl.Child->Asl.Next->Asl.Value.Integer;

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
        Flags |= ACPI_NS_EXTERNAL | ACPI_NS_DONT_OPEN_SCOPE;
        break;

    case PARSEOP_DEFAULT_ARG:

        if (Op->Asl.CompileFlags == OP_IS_RESOURCE_DESC)
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
            ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT, WalkState, &Node);
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_NOT_FOUND)
            {
                /* The name was not found, go ahead and create it */

                Status = AcpiNsLookup (WalkState->ScopeInfo, Path,
                    ACPI_TYPE_LOCAL_SCOPE, ACPI_IMODE_LOAD_PASS1,
                    Flags, WalkState, &Node);
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }

                /* However, this is an error -- operand to Scope must exist */

                if (strlen (Op->Asl.ExternalName) == ACPI_NAMESEG_SIZE)
                {
                    AslError (ASL_ERROR, ASL_MSG_NOT_FOUND, Op,
                        Op->Asl.ExternalName);
                }
                else
                {
                    AslError (ASL_ERROR, ASL_MSG_NAMEPATH_NOT_EXIST, Op,
                        Op->Asl.ExternalName);
                }

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
                (ACPI_COMPARE_NAMESEG (AslGbl_TableSignature, "DSDT")))
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
            sprintf (AslGbl_MsgBuffer, "%s [%s], changing type to [Scope]",
                Op->Asl.ExternalName, AcpiUtGetTypeName (Node->Type));
            AslError (ASL_REMARK, ASL_MSG_SCOPE_TYPE, Op, AslGbl_MsgBuffer);

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

            sprintf (AslGbl_MsgBuffer, "%s [%s]", Op->Asl.ExternalName,
                AcpiUtGetTypeName (Node->Type));
            AslError (ASL_ERROR, ASL_MSG_SCOPE_TYPE, Op, AslGbl_MsgBuffer);

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
     * For opcodes that enter new names into the namespace,
     * all prefix NameSegs must exist.
     */
    WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    if (((WalkState->OpInfo->Flags & AML_NAMED) ||
        (WalkState->OpInfo->Flags & AML_CREATE)) &&
        (Op->Asl.AmlOpcode != AML_EXTERNAL_OP))
    {
        Flags |= ACPI_NS_PREFIX_MUST_EXIST;
    }

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
            else if ((Node->Flags & ANOBJ_IS_EXTERNAL) ||
                     (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL))
            {
                Status = LdAnalyzeExternals (Node, Op, ObjectType, WalkState);
                if (ACPI_FAILURE (Status))
                {
                    if (Status == AE_ERROR)
                    {
                        /*
                         * The use of AE_ERROR here indicates that there was a
                         * compiler error emitted in LdAnalyzeExternals which
                         * means that the caller should proceed to the next Op
                         * for analysis of subsequent parse objects.
                         */
                        Status = AE_OK;
                    }
                    return_ACPI_STATUS (Status);
                }

                if (!(Node->Flags & ANOBJ_IS_EXTERNAL) &&
                     (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL))
                {
                    /*
                     * If we get to here, it means that an actual definition of
                     * the object declared external exists. Meaning that Op
                     * loading this this Op should have no change to the ACPI
                     * namespace. By going to FinishNode, we skip the
                     * assignment of Node->Op = Op.
                     */
                    goto FinishNode;
                }
            }
            else
            {
                /* Valid error, object already exists */

                ExternalPath = AcpiNsGetNormalizedPathname (Node, TRUE);

                AslDualParseOpError (ASL_ERROR, ASL_MSG_NAME_EXISTS, Op,
                    ExternalPath, ASL_MSG_FOUND_HERE, Node->Op,
                    ExternalPath);

                if (ExternalPath)
                {
                    ACPI_FREE (ExternalPath);
                }
                return_ACPI_STATUS (AE_OK);
            }
        }
        else if (AE_NOT_FOUND)
        {
            /*
             * One or more prefix NameSegs of the NamePath do not exist
             * (all of them must exist). Attempt to continue compilation
             * by setting the current scope to the root.
             */
            Node = AcpiGbl_RootNode;
            Status = AE_OK;
        }
        else
        {
            /* Flag all other errors as coming from the ACPICA core */

            AslCoreSubsystemError (Op, Status,
                "Failure from namespace lookup", FALSE);
            return_ACPI_STATUS (Status);
        }
    }

    /* Check special names like _WAK and _PTS */

    LdCheckSpecialNames (Node, Op);

    if (ForceNewScope)
    {
        Status = AcpiDsScopeStackPush (Node, ObjectType, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /* Point the Node back to the original Parse node */

    Node->Op = Op;

FinishNode:

    /* Point the parse node to the new namespace node */

    Op->Asl.Node = Node;

    if (Op->Asl.ParseOpcode == PARSEOP_METHOD)
    {
        /*
         * Get the method argument count from "Extra" and save
         * it in the namespace node
         */
        Node->Value = (UINT32) Op->Asl.Extra;
    }
    else if (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL &&
        Node->Type == ACPI_TYPE_METHOD &&
        (Node->Flags & ANOBJ_IS_EXTERNAL))
    {
        Node->Value =
            (UINT32) Op->Asl.Child->Asl.Next->Asl.Next->Asl.Value.Integer;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    LdMatchExternType
 *
 * PARAMETERS:  Type1
 *              Type2
 *
 * RETURN:      BOOLEAN
 *
 * DESCRIPTION: Match Type1 and Type2 with the assumption that one might be
 *              using external types and another might be using local types.
 *              This should be used to compare the types found in external
 *              declarations with types found in other external declarations or
 *              named object declaration. This should not be used to match two
 *              object type declarations.
 *
 ******************************************************************************/

static BOOLEAN
LdMatchExternType (
    ACPI_OBJECT_TYPE        Type1,
    ACPI_OBJECT_TYPE        Type2)
{
    BOOLEAN                 Type1IsLocal = Type1 > ACPI_TYPE_EXTERNAL_MAX;
    BOOLEAN                 Type2IsLocal = Type2 > ACPI_TYPE_EXTERNAL_MAX;
    ACPI_OBJECT_TYPE        ExternalType;
    ACPI_OBJECT_TYPE        LocalType;


    /*
     * The inputs could represent types that are local to ACPICA or types that
     * are known externally. Some local types, such as the OperationRegion
     * field units, are defined with more granularity than ACPICA local types.
     *
     * Therefore, map the local types to the external types before matching.
     */
    if (Type1IsLocal && !Type2IsLocal)
    {
        LocalType = Type1;
        ExternalType = Type2;
    }
    else if (!Type1IsLocal && Type2IsLocal)
    {
        LocalType = Type2;
        ExternalType = Type1;
    }
    else
    {
        return (Type1 == Type2);
    }

    switch (LocalType)
    {
        case ACPI_TYPE_LOCAL_REGION_FIELD:
        case ACPI_TYPE_LOCAL_BANK_FIELD:
        case ACPI_TYPE_LOCAL_INDEX_FIELD:

            LocalType = ACPI_TYPE_FIELD_UNIT;
            break;

        default:
            break;
    }

    return (LocalType == ExternalType);
}


/*******************************************************************************
 *
 * FUNCTION:    LdAnalyzeExternals
 *
 * PARAMETERS:  Node            - Node that represents the named object
 *              Op              - Named object declaring this named object
 *              ExternalOpType  - Type of ExternalOp
 *              WalkState       - Current WalkState
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Node and Op represents an identically named object declaration
 *              that is either declared by the ASL external keyword or declared
 *              by operators that declare named objects (i.e. Name, Device,
 *              OperationRegion, and etc.). This function ensures that the
 *              declarations do not contradict each other.
 *
 ******************************************************************************/

static ACPI_STATUS
LdAnalyzeExternals (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OBJECT_TYPE        ExternalOpType,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OBJECT_TYPE        ActualExternalOpType;
    ACPI_OBJECT_TYPE        ActualOpType;
    ACPI_PARSE_OBJECT       *ExternalOp;
    ACPI_PARSE_OBJECT       *ActualOp;


    /*
     * The declaration represented by Node and Op must have the same type.
     * The type of the external Op is represented by ExternalOpType. However,
     * the type of the pre-existing declaration depends on whether if Op
     * is an external declaration or an actual declaration.
     */
    if (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL)
    {
        ActualExternalOpType = ExternalOpType;
        ActualOpType = Node->Type;
    }
    else
    {
        ActualExternalOpType = Node->Type;
        ActualOpType = ExternalOpType;
    }

    if ((ActualOpType != ACPI_TYPE_ANY) &&
        (ActualExternalOpType != ACPI_TYPE_ANY) &&
        !LdMatchExternType (ActualExternalOpType, ActualOpType))
    {
        if (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL &&
            Node->Op->Asl.ParseOpcode == PARSEOP_EXTERNAL)
        {
            AslDualParseOpError (ASL_WARNING,
                ASL_MSG_DUPLICATE_EXTERN_MISMATCH, Op, NULL,
                ASL_MSG_DUPLICATE_EXTERN_FOUND_HERE, Node->Op, NULL);
        }
        else
        {
            if (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL &&
                Node->Op->Asl.ParseOpcode != PARSEOP_EXTERNAL)
            {
                ExternalOp = Op;
                ActualOp = Node->Op;
            }
            else
            {
                ExternalOp = Node->Op;
                ActualOp = Op;
            }
            AslDualParseOpError (ASL_WARNING,
                ASL_MSG_DECLARATION_TYPE_MISMATCH, ExternalOp, NULL,
                ASL_MSG_TYPE_MISMATCH_FOUND_HERE, ActualOp, NULL);
        }
    }

    /* Set the object type of the external */

    if ((Node->Flags & ANOBJ_IS_EXTERNAL) &&
        (Op->Asl.ParseOpcode != PARSEOP_EXTERNAL))
    {
        /*
         * Allow one create on an object or segment that was
         * previously declared External
         */
        Node->Flags &= ~ANOBJ_IS_EXTERNAL;
        Node->Type = (UINT8) ActualOpType;

        /* Just retyped a node, probably will need to open a scope */

        if (AcpiNsOpensScope (ActualOpType))
        {
            Status = AcpiDsScopeStackPush (Node, ActualOpType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
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
             (ActualOpType == ACPI_TYPE_ANY))
    {
        /* Allow update of externals of unknown type. */

        Node->Type = (UINT8) ActualExternalOpType;
        Status = AE_OK;
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    LdCheckSpecialNames
 *
 * PARAMETERS:  Node        - Node that represents the named object
 *              Op          - Named object declaring this named object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check if certain named objects are declared in the incorrect
 *              scope. Special named objects are listed in
 *              AslGbl_SpecialNamedObjects and can only be declared at the root
 *              scope. _UID inside of a processor declaration must not be a
 *              string.
 *
 ******************************************************************************/

static void
LdCheckSpecialNames (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  i;


    for (i = 0; i < MAX_SPECIAL_NAMES; i++)
    {
        if (ACPI_COMPARE_NAMESEG(Node->Name.Ascii, AslGbl_SpecialNamedObjects[i]) &&
            Node->Parent != AcpiGbl_RootNode)
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_SPECIAL_NAME, Op, Op->Asl.ExternalName);
            return;
        }
    }

    if (ACPI_COMPARE_NAMESEG (Node->Name.Ascii, "_UID") &&
        Node->Parent->Type == ACPI_TYPE_PROCESSOR &&
        Node->Type == ACPI_TYPE_STRING)
    {
        AslError (ASL_ERROR, ASL_MSG_INVALID_PROCESSOR_UID , Op, "found a string");
    }
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
        (Op->Asl.CompileFlags == OP_IS_RESOURCE_DESC))
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
        if (Op->Asl.CompileFlags & OP_IS_RESOURCE_DESC)
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
        /*
         * Complete the alias node by getting and saving the target node.
         * First child is the alias target
         */
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
                /* Standalone NameSeg vs. NamePath */

                if (strlen (Arg->Asl.ExternalName) == ACPI_NAMESEG_SIZE)
                {
                    AslError (ASL_ERROR, ASL_MSG_NOT_FOUND, Op,
                        Arg->Asl.ExternalName);
                }
                else
                {
                    AslError (ASL_ERROR, ASL_MSG_NAMEPATH_NOT_EXIST, Op,
                        Arg->Asl.ExternalName);
                }

#if 0
/*
 * NOTE: Removed 10/2018 to enhance compiler error reporting. No
 * regressions seen.
 */
                /*
                 * The name was not found, go ahead and create it.
                 * This prevents more errors later.
                 */
                Status = AcpiNsLookup (WalkState->ScopeInfo, Path,
                    ACPI_TYPE_ANY, ACPI_IMODE_LOAD_PASS1,
                    ACPI_NS_NO_UPSEARCH, WalkState, &Node);
#endif
                return (Status);
/* Removed: return (AE_OK)*/
            }

            AslCoreSubsystemError (Op, Status,
                "Failure from namespace lookup", FALSE);
            return (AE_OK);
        }

        /* Save the target node within the alias node as well as type information */

        Node->Object = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, TargetNode);
        Node->Type = TargetNode->Type;
        if (Node->Type == ACPI_TYPE_METHOD)
        {
            /* Save the parameter count for methods */

            Node->Value = TargetNode->Value;
        }
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
        (Op->Asl.CompileFlags == OP_IS_RESOURCE_DESC))
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
        if (Op->Asl.CompileFlags & OP_IS_RESOURCE_DESC)
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
