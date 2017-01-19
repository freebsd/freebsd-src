/******************************************************************************
 *
 * Module Name: aslxrefout.c - support for optional cross-reference file
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
#include "acnamesp.h"
#include "acparser.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslxrefout")


/* Local prototypes */

static ACPI_STATUS
OtXrefWalkPart2 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
OtXrefWalkPart3 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
OtXrefAnalysisWalkPart1 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


static ACPI_STATUS
OtXrefAnalysisWalkPart2 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
OtXrefAnalysisWalkPart3 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*******************************************************************************
 *
 * FUNCTION:    OtPrintHeaders
 *
 * PARAMETERS:  Message             - Main header message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emits the main header message along with field descriptions
 *
 ******************************************************************************/

void
OtPrintHeaders (
    char                    *Message)
{
    UINT32                  Length;


    Length = strlen (Message);

    FlPrintFile (ASL_FILE_XREF_OUTPUT, "\n\n%s\n", Message);
    while (Length)
    {
        FlPrintFile (ASL_FILE_XREF_OUTPUT, "-");
        Length--;
    }

    FlPrintFile (ASL_FILE_XREF_OUTPUT, "\n\nLineno   %-40s Description\n",
        "Full Pathname");
}


/*******************************************************************************
 *
 * FUNCTION:    OtCreateXrefFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION  Main entry point for parts 2 and 3 of the cross-reference
 *              file.
 *
 ******************************************************************************/

void
OtCreateXrefFile (
    void)
{
    ASL_XREF_INFO           XrefInfo;


    /* Build cross-reference output file if requested */

    if (!Gbl_CrossReferenceOutput)
    {
        return;
    }

    memset (&XrefInfo, 0, sizeof (ASL_XREF_INFO));

    /* Cross-reference output file, part 2 (Method invocations) */

    OtPrintHeaders ("Part 2: Method Reference Map "
        "(Invocations of each user-defined control method)");

    TrWalkParseTree (Gbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
        OtXrefWalkPart2, NULL, &XrefInfo);

    /* Cross-reference output file, part 3 (All other object refs) */

    OtPrintHeaders ("Part 3: Full Object Reference Map "
        "(Methods that reference each object in namespace");

    TrWalkParseTree (Gbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
        OtXrefWalkPart3, NULL, &XrefInfo);

    /* Cross-reference summary */

    FlPrintFile (ASL_FILE_XREF_OUTPUT, "\n\nObject Summary\n");

    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "\nTotal methods:                   %u\n",
        XrefInfo.TotalPredefinedMethods + XrefInfo.TotalUserMethods);
    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "Total predefined methods:        %u\n",
        XrefInfo.TotalPredefinedMethods);

    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "\nTotal user methods:              %u\n",
        XrefInfo.TotalUserMethods);
    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "Total unreferenced user methods  %u\n",
        XrefInfo.TotalUnreferenceUserMethods);

    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "\nTotal defined objects:           %u\n",
        XrefInfo.TotalObjects);
    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "Total unreferenced objects:      %u\n",
        XrefInfo.TotalUnreferencedObjects);
}


/*
 * Part 1 of the cross reference file. This part emits the namespace objects
 * that are referenced by each control method in the namespace.
 *
 * Part 2 and 3 are below part 1.
 */

/*******************************************************************************
 *
 * FUNCTION:    OtXrefWalkPart1
 *
 * PARAMETERS:  Op                      - Current parse Op
 *              Level                   - Current tree nesting level
 *              MethodInfo              - Info block for the current method
 *
 *
 * RETURN:      None
 *
 * DESCRIPTION: Entry point for the creation of the method call reference map.
 *              For each control method in the namespace, all other methods
 *              that invoke the method are listed. Predefined names/methods
 *              that start with an underscore are ignored, because these are
 *              essentially external/public interfaces.

 * DESCRIPTION: Entry point for the creation of the object reference map.
 *              For each control method in the namespace, all objects that
 *              are referenced by the method are listed.
 *
 *              Called during a normal namespace walk, once per namespace
 *              object. (MtMethodAnalysisWalkBegin)
 *
 ******************************************************************************/

void
OtXrefWalkPart1 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    ASL_METHOD_INFO         *MethodInfo)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_PARSE_OBJECT       *FieldOp;
    char                    *ParentPath;
    UINT32                  Length;
    ACPI_STATUS             Status;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_NAMESEG:
    case PARSEOP_NAMESTRING:
    case PARSEOP_METHODCALL:

        if (!MethodInfo ||
            (MethodInfo->Op->Asl.Child == Op) ||
            !Op->Asl.Node)
        {
            break;
        }

        MethodInfo->CurrentOp = Op;
        Node = Op->Asl.Node;

        /* Find all objects referenced by this method */

        Status = TrWalkParseTree (MethodInfo->Op, ASL_WALK_VISIT_DOWNWARD,
            OtXrefAnalysisWalkPart1, NULL, MethodInfo);

        if (Status == AE_CTRL_TERMINATE)
        {
            ParentPath = AcpiNsGetNormalizedPathname (Node, TRUE);

            FlPrintFile (ASL_FILE_XREF_OUTPUT, "            %-40s %s",
                ParentPath, AcpiUtGetTypeName (Node->Type));
            ACPI_FREE (ParentPath);

            switch (Node->Type)
            {
                /* Handle externals */

            case ACPI_TYPE_ANY:
            case ACPI_TYPE_FIELD_UNIT:

                FlPrintFile (ASL_FILE_XREF_OUTPUT, " <External Object>");
                break;

            case ACPI_TYPE_INTEGER:

                FlPrintFile (ASL_FILE_XREF_OUTPUT, " %8.8X%8.8X",
                    ACPI_FORMAT_UINT64 (Op->Asl.Value.Integer));
                break;

            case ACPI_TYPE_METHOD:

                FlPrintFile (ASL_FILE_XREF_OUTPUT, " Invocation (%u args)",
                    Node->ArgCount);
                break;

            case ACPI_TYPE_BUFFER_FIELD:

                NextOp = Node->Op;              /* Create Buffer Field Op */
                switch (NextOp->Asl.ParseOpcode)
                {
                case PARSEOP_CREATEBITFIELD:
                    Length = 1;
                    break;

                case PARSEOP_CREATEBYTEFIELD:
                    Length = 8;
                    break;

                case PARSEOP_CREATEWORDFIELD:
                    Length = 16;
                    break;

                case PARSEOP_CREATEDWORDFIELD:
                    Length = 32;
                    break;

                case PARSEOP_CREATEQWORDFIELD:
                    Length = 64;
                    break;

                default:
                    Length = 0;
                    break;
                }

                NextOp = NextOp->Asl.Child;     /* Buffer name */

                if (!NextOp->Asl.ExternalName)
                {
                    FlPrintFile (ASL_FILE_XREF_OUTPUT, " in Arg/Local");
                }
                else
                {
                    ParentPath = AcpiNsGetNormalizedPathname (
                        NextOp->Asl.Node, TRUE);

                    FlPrintFile (ASL_FILE_XREF_OUTPUT, " (%.2u bit) in Buffer %s",
                        Length, ParentPath);
                    ACPI_FREE (ParentPath);
                }
                break;

            case ACPI_TYPE_LOCAL_REGION_FIELD:

                NextOp = Node->Op;
                FieldOp = NextOp->Asl.Parent;
                NextOp = FieldOp->Asl.Child;

                ParentPath = AcpiNsGetNormalizedPathname (
                    NextOp->Asl.Node, TRUE);

                FlPrintFile (ASL_FILE_XREF_OUTPUT, " (%.2u bit) in Region %s",
                    (UINT32) Node->Op->Asl.Child->Asl.Value.Integer,
                    ParentPath);
                ACPI_FREE (ParentPath);

                if (FieldOp->Asl.ParseOpcode == PARSEOP_FIELD)
                {
                    Node = NextOp->Asl.Node;        /* Region node */
                    NextOp = Node->Op;              /* PARSEOP_REGION */
                    NextOp = NextOp->Asl.Child;     /* Region name */
                    NextOp = NextOp->Asl.Next;

                    /* Get region space/addr/len? */

                    FlPrintFile (ASL_FILE_XREF_OUTPUT, " (%s)",
                        AcpiUtGetRegionName ((UINT8)
                        NextOp->Asl.Value.Integer));
                }
                break;

            default:
                break;
            }

            FlPrintFile (ASL_FILE_XREF_OUTPUT, "\n");
        }
        break;

    case PARSEOP_METHOD:

        ParentPath = AcpiNsGetNormalizedPathname (Op->Asl.Node, TRUE);

        FlPrintFile (ASL_FILE_XREF_OUTPUT,
            "\n[%5u]  %-40s %s Declaration (%u args)\n",
            Op->Asl.LogicalLineNumber, ParentPath,
            AcpiUtGetTypeName (Op->Asl.Node->Type), Op->Asl.Node->ArgCount);

        ACPI_FREE (ParentPath);
        break;

    default:
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    OtXrefAnalysisWalkPart1
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Secondary walk for cross-reference part 1.
 *
 ******************************************************************************/

static ACPI_STATUS
OtXrefAnalysisWalkPart1 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_METHOD_INFO         *MethodInfo = (ASL_METHOD_INFO *) Context;
    ACPI_PARSE_OBJECT       *Next;


    /* Only interested in name string Ops -- ignore all others */

    if ((Op->Asl.ParseOpcode != PARSEOP_NAMESEG) &&
        (Op->Asl.ParseOpcode != PARSEOP_NAMESTRING) &&
        (Op->Asl.ParseOpcode != PARSEOP_METHODCALL))
    {
        return (AE_OK);
    }

    /* No node means a locally declared object -- ignore */

    if (!Op->Asl.Node)
    {
        return (AE_OK);
    }

    /* When we encounter the source Op, we are done */

    Next = MethodInfo->CurrentOp;
    if (Next == Op)
    {
        return (AE_CTRL_TERMINATE);
    }

    /* If we have a name match, this Op is a duplicate */

    if ((Next->Asl.ParseOpcode == PARSEOP_NAMESEG)      ||
        (Next->Asl.ParseOpcode == PARSEOP_NAMESTRING)   ||
        (Next->Asl.ParseOpcode == PARSEOP_METHODCALL))
    {
        if (!strcmp (Op->Asl.ExternalName, Next->Asl.ExternalName))
        {
            return (AE_ALREADY_EXISTS);
        }
    }

    return (AE_OK);
}


/*
 * Part 2 of the cross reference file. This part emits the names of each
 * non-predefined method in the namespace (user methods), along with the
 * names of each control method that references that method.
 */

/*******************************************************************************
 *
 * FUNCTION:    OtXrefWalkPart2
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: For each control method in the namespace, we will re-walk the
 *              namespace to find each and every invocation of that control
 *              method. Brute force, but does not matter, even for large
 *              namespaces. Ignore predefined names (start with underscore).
 *
 ******************************************************************************/

static ACPI_STATUS
OtXrefWalkPart2 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_XREF_INFO           *XrefInfo = (ASL_XREF_INFO *) Context;
    ACPI_NAMESPACE_NODE     *Node;
    char                    *ParentPath;


    /* Looking for Method Declaration Ops only */

    if (!Op->Asl.Node ||
        (Op->Asl.ParseOpcode != PARSEOP_METHOD))
    {
        return (AE_OK);
    }

    /* Ignore predefined names */

    if (Op->Asl.Node->Name.Ascii[0] == '_')
    {
        XrefInfo->TotalPredefinedMethods++;
        return (AE_OK);
    }

    Node = Op->Asl.Node;
    ParentPath = AcpiNsGetNormalizedPathname (Node, TRUE);

    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "\n[%5u]  %-40s %s Declaration (%u args)\n",
        Op->Asl.LogicalLineNumber, ParentPath,
        AcpiUtGetTypeName (Node->Type), Node->ArgCount);

    XrefInfo->TotalUserMethods++;
    XrefInfo->ThisMethodInvocations = 0;
    XrefInfo->MethodOp = Op;

    (void) TrWalkParseTree (Gbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
        OtXrefAnalysisWalkPart2, NULL, XrefInfo);

    if (!XrefInfo->ThisMethodInvocations)
    {
        FlPrintFile (ASL_FILE_XREF_OUTPUT,
            "            Zero invocations of this method in this module\n");
        XrefInfo->TotalUnreferenceUserMethods++;
    }
    else
    {
        FlPrintFile (ASL_FILE_XREF_OUTPUT,
            "            %u invocations of method %s in this module\n",
            XrefInfo->ThisMethodInvocations, ParentPath);
    }

    ACPI_FREE (ParentPath);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    OtXrefAnalysisWalkPart2
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: For every Op that is a method invocation, emit a reference
 *              line if the Op is invoking the target method.
 *
 ******************************************************************************/

static ACPI_STATUS
OtXrefAnalysisWalkPart2 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_XREF_INFO           *XrefInfo = (ASL_XREF_INFO *) Context;
    ACPI_PARSE_OBJECT       *CallerOp;
    char                    *CallerFullPathname;


    /* Looking for MethodCall Ops only */

    if (!Op->Asl.Node ||
        (Op->Asl.ParseOpcode != PARSEOP_METHODCALL))
    {
        return (AE_OK);
    }

    /* If not a match to the target method, we are done */

    if (Op->Asl.Node != XrefInfo->MethodOp->Asl.Node)
    {
        return (AE_CTRL_DEPTH);
    }

    /* Find parent method to get method caller namepath */

    CallerOp = Op->Asl.Parent;
    while (CallerOp &&
        (CallerOp->Asl.ParseOpcode != PARSEOP_METHOD))
    {
        CallerOp = CallerOp->Asl.Parent;
    }

    /* There is no parent method for External() statements */

    if (!CallerOp)
    {
        return (AE_OK);
    }

    CallerFullPathname = AcpiNsGetNormalizedPathname (
        CallerOp->Asl.Node, TRUE);

    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "[%5u]     %-40s Invocation path: %s\n",
        Op->Asl.LogicalLineNumber, CallerFullPathname,
        Op->Asl.ExternalName);

    ACPI_FREE (CallerFullPathname);
    XrefInfo->ThisMethodInvocations++;
    return (AE_OK);
}


/*
 * Part 3 of the cross reference file. This part emits the names of each
 * non-predefined method in the namespace (user methods), along with the
 * names of each control method that references that method.
 */

/*******************************************************************************
 *
 * FUNCTION:    OtXrefWalkPart3
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Cross-reference part 3. references to objects other than
 *              control methods.
 *
 ******************************************************************************/

static ACPI_STATUS
OtXrefWalkPart3 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_XREF_INFO           *XrefInfo = (ASL_XREF_INFO *) Context;
    ACPI_NAMESPACE_NODE     *Node;
    char                    *ParentPath;
    const ACPI_OPCODE_INFO  *OpInfo;


    /* Ignore method declarations */

    if (!Op->Asl.Node ||
        (Op->Asl.ParseOpcode == PARSEOP_METHOD))
    {
        return (AE_OK);
    }

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    if (!(OpInfo->Class & AML_CLASS_NAMED_OBJECT))
    {
        return (AE_OK);
    }

    /* Only care about named object creation opcodes */

    if ((Op->Asl.ParseOpcode != PARSEOP_NAME) &&
        (Op->Asl.ParseOpcode != PARSEOP_DEVICE) &&
        (Op->Asl.ParseOpcode != PARSEOP_MUTEX) &&
        (Op->Asl.ParseOpcode != PARSEOP_OPERATIONREGION) &&
        (Op->Asl.ParseOpcode != PARSEOP_FIELD) &&
        (Op->Asl.ParseOpcode != PARSEOP_EVENT))
    {
        return (AE_OK);
    }

    /* Ignore predefined names */

    if (Op->Asl.Node->Name.Ascii[0] == '_')
    {
        return (AE_OK);
    }

    Node = Op->Asl.Node;
    ParentPath = AcpiNsGetNormalizedPathname (Node, TRUE);

    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "\n[%5u]  %-40s %s Declaration\n",
        Op->Asl.LogicalLineNumber, ParentPath,
        AcpiUtGetTypeName (Node->Type));
    ACPI_FREE (ParentPath);

    XrefInfo->MethodOp = Op;
    XrefInfo->ThisObjectReferences = 0;
    XrefInfo->TotalObjects = 0;

    (void) TrWalkParseTree (Gbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
        OtXrefAnalysisWalkPart3, NULL, XrefInfo);

    if (!XrefInfo->ThisObjectReferences)
    {
        FlPrintFile (ASL_FILE_XREF_OUTPUT,
            "            Zero references to this object in this module\n");
        XrefInfo->TotalUnreferencedObjects++;
    }
    else
    {
        FlPrintFile (ASL_FILE_XREF_OUTPUT,
            "            %u references to this object in this module\n",
            XrefInfo->ThisObjectReferences, ParentPath);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    OtXrefAnalysisWalkPart3
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Secondary walk for cross-reference part 3.
 *
 ******************************************************************************/

static ACPI_STATUS
OtXrefAnalysisWalkPart3 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_XREF_INFO           *XrefInfo = (ASL_XREF_INFO *) Context;
    char                    *CallerFullPathname = NULL;
    ACPI_PARSE_OBJECT       *CallerOp;
    const char              *Operator;


    if (!Op->Asl.Node)
    {
        return (AE_OK);
    }

    XrefInfo->TotalObjects++;

    /* Ignore Op that actually defined the object */

    if (Op == XrefInfo->MethodOp)
    {
        return (AE_OK);
    }

    /* Only interested in Ops that reference the target node */

    if (Op->Asl.Node != XrefInfo->MethodOp->Asl.Node)
    {
        return (AE_OK);
    }

    /* Find parent "open scope" object to get method caller namepath */

    CallerOp = Op->Asl.Parent;
    while (CallerOp &&
        (CallerOp->Asl.ParseOpcode != PARSEOP_NAME) &&
        (CallerOp->Asl.ParseOpcode != PARSEOP_METHOD) &&
        (CallerOp->Asl.ParseOpcode != PARSEOP_DEVICE) &&
        (CallerOp->Asl.ParseOpcode != PARSEOP_POWERRESOURCE) &&
        (CallerOp->Asl.ParseOpcode != PARSEOP_PROCESSOR) &&
        (CallerOp->Asl.ParseOpcode != PARSEOP_THERMALZONE))
    {
        CallerOp = CallerOp->Asl.Parent;
    }

    if (CallerOp == XrefInfo->CurrentMethodOp)
    {
        return (AE_OK);
    }

    /* Null CallerOp means the caller is at the namespace root */

    if (CallerOp)
    {
        CallerFullPathname = AcpiNsGetNormalizedPathname (
            CallerOp->Asl.Node, TRUE);
    }

    /* There are some special cases for the oddball operators */

    if (Op->Asl.ParseOpcode == PARSEOP_SCOPE)
    {
        Operator = "Scope";
    }
    else if (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_ALIAS)
    {
        Operator = "Alias";
    }
    else if (!CallerOp)
    {
        Operator = "ModLevel";
    }
    else
    {
        Operator = AcpiUtGetTypeName (CallerOp->Asl.Node->Type);
    }

    FlPrintFile (ASL_FILE_XREF_OUTPUT,
        "[%5u]     %-40s %-8s via path: %s, Operator: %s\n",
        Op->Asl.LogicalLineNumber,
        CallerFullPathname ? CallerFullPathname : "<root>",
        Operator,
        Op->Asl.ExternalName,
        Op->Asl.Parent->Asl.ParseOpName);

    if (!CallerOp)
    {
        CallerOp = ACPI_TO_POINTER (0xFFFFFFFF);
    }

    if (CallerFullPathname)
    {
        ACPI_FREE (CallerFullPathname);
    }

    XrefInfo->CurrentMethodOp = CallerOp;
    XrefInfo->ThisObjectReferences++;
    return (AE_OK);
}
