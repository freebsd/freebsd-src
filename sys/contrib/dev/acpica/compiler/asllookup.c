/******************************************************************************
 *
 * Module Name: asllookup- Namespace lookup
 *              $Revision: 1.95 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code.  No other license or right
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
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
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
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
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
 *****************************************************************************/


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"

#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acdispat.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asllookup")

/* Local prototypes */

static ACPI_STATUS
LsCompareOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
LsDoOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static BOOLEAN
LkObjectExists (
    char                    *Name);

static void
LkCheckFieldRange (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  RegionBitLength,
    UINT32                  FieldBitOffset,
    UINT32                  FieldBitLength,
    UINT32                  AccessBitWidth);

static ACPI_STATUS
LkNamespaceLocateBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
LkNamespaceLocateEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*******************************************************************************
 *
 * FUNCTION:    LsDoOneNamespaceObject
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Dump a namespace object to the namespace output file.
 *              Called during the walk of the namespace to dump all objects.
 *
 ******************************************************************************/

static ACPI_STATUS
LsDoOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARSE_OBJECT       *Op;


    Gbl_NumNamespaceObjects++;

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "%5d  [%d]  %*s %4.4s - %s",
                        Gbl_NumNamespaceObjects, Level, (Level * 3), " ",
                        &Node->Name,
                        AcpiUtGetTypeName (Node->Type));

    Op = Node->Op;
    ObjDesc = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, Node->Object);

    if (!Op)
    {
        FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "\n");
        return (AE_OK);
    }


    if ((ObjDesc) &&
        (ObjDesc->Common.Descriptor == ACPI_DESC_TYPE_OPERAND))
    {
        switch (Node->Type)
        {
        case ACPI_TYPE_INTEGER:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "       [Initial Value   0x%8.8X%8.8X]",
                ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
            break;


        case ACPI_TYPE_STRING:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "        [Initial Value   \"%s\"]",
                ObjDesc->String.Pointer);
            break;

        default:
            /* Nothing to do for other types */
            break;
        }

    }
    else
    {
        switch (Node->Type)
        {
        case ACPI_TYPE_INTEGER:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "       [Initial Value   0x%8.8X%8.8X]",
                ACPI_FORMAT_UINT64 (Op->Asl.Value.Integer));
            break;


        case ACPI_TYPE_STRING:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "        [Initial Value   \"%s\"]",
                Op->Asl.Value.String);
            break;


        case ACPI_TYPE_LOCAL_REGION_FIELD:

            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Child;
            }
            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "   [Offset 0x%04X   Length 0x%04X bits]",
                Op->Asl.Parent->Asl.ExtraValue, (UINT32) Op->Asl.Value.Integer);
            break;


        case ACPI_TYPE_BUFFER_FIELD:

            switch (Op->Asl.ParseOpcode)
            {
            case PARSEOP_CREATEBYTEFIELD:
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [BYTE  ( 8 bit)]");
                break;

            case PARSEOP_CREATEDWORDFIELD:
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [DWORD (32 bit)]");
                break;

            case PARSEOP_CREATEQWORDFIELD:
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [QWORD (64 bit)]");
                break;

            case PARSEOP_CREATEWORDFIELD:
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [WORD  (16 bit)]");
                break;

            case PARSEOP_CREATEBITFIELD:
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [BIT   ( 1 bit)]");
                break;

            case PARSEOP_CREATEFIELD:
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [Arbitrary Bit Field]");
                break;

            default:
                break;

            }
            break;


        case ACPI_TYPE_PACKAGE:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            Op = Op->Asl.Child;

            if ((Op->Asl.ParseOpcode == PARSEOP_BYTECONST) ||
                (Op->Asl.ParseOpcode == PARSEOP_RAW_DATA))
            {
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                    "       [Initial Length  0x%.2X elements]",
                    Op->Asl.Value.Integer);
            }
            break;


        case ACPI_TYPE_BUFFER:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            Op = Op->Asl.Child;

            if (Op->Asl.ParseOpcode == PARSEOP_INTEGER)
            {
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                    "        [Initial Length  0x%.2X bytes]",
                    Op->Asl.Value.Integer);
            }
            break;


        case ACPI_TYPE_METHOD:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "        [Code Length     0x%.4X bytes]",
                Op->Asl.AmlSubtreeLength);
            break;


        default:
            /* Nothing to do for other types */
            break;
        }
    }

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LsDisplayNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk the namespace an display information about each node
 *              in the tree.  Information is written to the optional
 *              namespace output file.
 *
 ******************************************************************************/

ACPI_STATUS
LsDisplayNamespace (
    void)
{
    ACPI_STATUS             Status;


    if (!Gbl_NsOutputFlag)
    {
        return (AE_OK);
    }

    /* File header */

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "Contents of ACPI Namespace\n\n");
    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "Count  Depth    Name - Type\n\n");

    /* Walk entire namespace from the root */

    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                ACPI_UINT32_MAX, FALSE, LsDoOneNamespaceObject,
                NULL, NULL);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    LsCompareOneNamespaceObject
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compare name of one object.
 *
 ******************************************************************************/

static ACPI_STATUS
LsCompareOneNamespaceObject (
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
 * FUNCTION:    LkObjectExists
 *
 * PARAMETERS:  Name            - 4 char ACPI name
 *
 * RETURN:      TRUE if name exists in namespace
 *
 * DESCRIPTION: Walk the namespace to find an object
 *
 ******************************************************************************/

static BOOLEAN
LkObjectExists (
    char                    *Name)
{
    ACPI_STATUS             Status;


    /* Walk entire namespace from the supplied root */

    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                ACPI_UINT32_MAX, FALSE, LsCompareOneNamespaceObject,
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
 * FUNCTION:    LkCrossReferenceNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform a cross reference check of the parse tree against the
 *              namespace.  Every named referenced within the parse tree
 *              should be get resolved with a namespace lookup.  If not, the
 *              original reference in the ASL code is invalid -- i.e., refers
 *              to a non-existent object.
 *
 * NOTE:  The ASL "External" operator causes the name to be inserted into the
 *        namespace so that references to the external name will be resolved
 *        correctly here.
 *
 ******************************************************************************/

ACPI_STATUS
LkCrossReferenceNamespace (
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
        return AE_NO_MEMORY;
    }

    /* Walk the entire parse tree */

    TrWalkParseTree (RootNode, ASL_WALK_VISIT_TWICE, LkNamespaceLocateBegin,
                        LkNamespaceLocateEnd, WalkState);
    return AE_OK;
}


/*******************************************************************************
 *
 * FUNCTION:    LkCheckFieldRange
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
LkCheckFieldRange (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  RegionBitLength,
    UINT32                  FieldBitOffset,
    UINT32                  FieldBitLength,
    UINT32                  AccessBitWidth)
{
    UINT32                  FieldEndBitOffset;


    /*
     * Check each field unit against the region size.  The entire
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
     * the end-of-region.  Assumes AccessBitWidth is a power of 2
     */
    FieldEndBitOffset = ACPI_ROUND_UP (FieldEndBitOffset, AccessBitWidth);

    if (FieldEndBitOffset > RegionBitLength)
    {
        /* Field definition combined with the access is beyond EOR */

        AslError (ASL_ERROR, ASL_MSG_FIELD_UNIT_ACCESS_WIDTH, Op, NULL);
    }
}

/*******************************************************************************
 *
 * FUNCTION:    LkNamespaceLocateBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during cross-reference.  For named
 *              object references, attempt to locate the name in the
 *              namespace.
 *
 * NOTE: ASL references to named fields within resource descriptors are
 *       resolved to integer values here.  Therefore, this step is an
 *       important part of the code generation.  We don't know that the
 *       name refers to a resource descriptor until now.
 *
 ******************************************************************************/

static ACPI_STATUS
LkNamespaceLocateBegin (
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
    UINT32                  Temp;
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  Flags;


    ACPI_FUNCTION_TRACE_PTR ("LkNamespaceLocateBegin", Op);

    /*
     * If this node is the actual declaration of a name
     * [such as the XXXX name in "Method (XXXX)"],
     * we are not interested in it here.  We only care about names that are
     * references to other objects within the namespace and the parent objects
     * of name declarations
     */
    if (Op->Asl.CompileFlags & NODE_IS_NAME_DECLARATION)
    {
        return (AE_OK);
    }

    /* We are only interested in opcodes that have an associated name */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);

    if ((!(OpInfo->Flags & AML_NAMED)) &&
        (!(OpInfo->Flags & AML_CREATE)) &&
        (Op->Asl.ParseOpcode != PARSEOP_NAMESTRING) &&
        (Op->Asl.ParseOpcode != PARSEOP_NAMESEG)    &&
        (Op->Asl.ParseOpcode != PARSEOP_METHODCALL))
    {
        return (AE_OK);
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
        /* For all NAMED operators, the name reference is the first child */

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
     * Lookup the name in the namespace.  Name must exist at this point, or it
     * is an invalid reference.
     *
     * The namespace is also used as a lookup table for references to resource
     * descriptors and the fields within them.
     */
    Gbl_NsLookupCount++;

    Status = AcpiNsLookup (WalkState->ScopeInfo,  Path, ObjectType,
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

                if (LkObjectExists (Path))
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

                    if ((Op->Asl.Parent) &&
                        (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_CONDREFOF))
                    {
                        /* Ignore not found if parent is CondRefOf */

                        return (AE_OK);
                    }

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

                    if ((Op->Asl.Parent) &&
                        (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_CONDREFOF))
                    {
                        /* Ignore not found if parent is CondRefOf */

                        return (AE_OK);
                    }

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
        return (Status);
    }

    /* Attempt to optimize the NamePath */

    OptOptimizeNamePath (Op, OpInfo->Flags, WalkState, Path, Node);

    /*
     * Dereference an alias. (A name reference that is an alias.)
     * Aliases are not nested;  The alias always points to the final object
     */
    if ((Op->Asl.ParseOpcode != PARSEOP_ALIAS) &&
        (Node->Type == ACPI_TYPE_LOCAL_ALIAS))
    {
        /* This node points back to the original PARSEOP_ALIAS */

        NextOp = Node->Op;

        /* The first child is the alias target op */

        NextOp = NextOp->Asl.Child;

        /* Who in turn points back to original target alias node */

        if (NextOp->Asl.Node)
        {
            Node = NextOp->Asl.Node;
        }
        else
        {
            AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Op,
                "Missing alias link");
        }
    }

    /* 1) Check for a reference to a resource descriptor */

    else if ((Node->Type == ACPI_TYPE_LOCAL_RESOURCE_FIELD) ||
             (Node->Type == ACPI_TYPE_LOCAL_RESOURCE))
    {
        /*
         * This was a reference to a field within a resource descriptor.  Extract
         * the associated field offset (either a bit or byte offset depending on
         * the field type) and change the named reference into an integer for
         * AML code generation
         */
        Temp = Node->Value;
        if (Node->Flags & ANOBJ_IS_BIT_OFFSET)
        {
            Op->Asl.CompileFlags |= NODE_IS_BIT_OFFSET;
        }

        /* Perform BitOffset <--> ByteOffset conversion if necessary */

        switch (Op->Asl.Parent->Asl.AmlOpcode)
        {
        case AML_CREATE_FIELD_OP:

            /* We allow a Byte offset to Bit Offset conversion for this op */

            if (!(Op->Asl.CompileFlags & NODE_IS_BIT_OFFSET))
            {
                /* Simply multiply byte offset times 8 to get bit offset */

                Temp = ACPI_MUL_8 (Temp);
            }
            break;


        case AML_CREATE_BIT_FIELD_OP:

            /* This op requires a Bit Offset */

            if (!(Op->Asl.CompileFlags & NODE_IS_BIT_OFFSET))
            {
                AslError (ASL_ERROR, ASL_MSG_BYTES_TO_BITS, Op, NULL);
            }
            break;


        case AML_CREATE_BYTE_FIELD_OP:
        case AML_CREATE_WORD_FIELD_OP:
        case AML_CREATE_DWORD_FIELD_OP:
        case AML_CREATE_QWORD_FIELD_OP:
        case AML_INDEX_OP:

            /* These Ops require Byte offsets */

            if (Op->Asl.CompileFlags & NODE_IS_BIT_OFFSET)
            {
                AslError (ASL_ERROR, ASL_MSG_BITS_TO_BYTES, Op, NULL);
            }
            break;


        default:
            /* Nothing to do for other opcodes */
            break;
        }

        /* Now convert this node to an integer whose value is the field offset */

        Op->Asl.AmlLength       = 0;
        Op->Asl.ParseOpcode     = PARSEOP_INTEGER;
        Op->Asl.Value.Integer   = (UINT64) Temp;
        Op->Asl.CompileFlags   |= NODE_IS_RESOURCE_FIELD;

        OpcGenerateAmlOpcode (Op);
    }

    /* 2) Check for a method invocation */

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
            return (AE_OK);
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
            return (AE_OK);
        }

        /* Save the method node in the caller's op */

        Op->Asl.Node = Node;
        if (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_CONDREFOF)
        {
            return (AE_OK);
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
                sprintf (MsgBuffer, "%s requires %d", Op->Asl.ExternalName,
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

    /* 3) Check for an ASL Field definition */

    else if ((Op->Asl.Parent) &&
            ((Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_FIELD)     ||
             (Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_BANKFIELD)))
    {
        /*
         * Offset checking for fields.  If the parent operation region has a
         * constant length (known at compile time), we can check fields
         * defined in that region against the region length.  This will catch
         * fields and field units that cannot possibly fit within the region.
         *
         * Note: Index fields do not directly reference an operation region,
         * thus they are not included in this check.
         */
        if (Op == Op->Asl.Parent->Asl.Child)
        {
            /*
             * This is the first child of the field node, which is
             * the name of the region.  Get the parse node for the
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
            case REGION_EC:
            case REGION_CMOS:

                if ((UINT8) Op->Asl.Parent->Asl.Value.Integer != AML_FIELD_ACCESS_BYTE)
                {
                    AslError (ASL_ERROR, ASL_MSG_REGION_BYTE_ACCESS, Op, NULL);
                }
                break;

            case REGION_SMBUS:

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
             * This is one element of the field list.  Check to make sure
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
                LkCheckFieldRange (Op,
                            Op->Asl.Parent->Asl.ExtraValue,
                            Op->Asl.ExtraValue,
                            (UINT32) Op->Asl.Child->Asl.Value.Integer,
                            Op->Asl.Child->Asl.ExtraValue);
            }
        }
    }

    Op->Asl.Node = Node;
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    LkNamespaceLocateEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during cross reference.  We only
 *              need to worry about scope management here.
 *
 ******************************************************************************/

static ACPI_STATUS
LkNamespaceLocateEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    const ACPI_OPCODE_INFO  *OpInfo;


    ACPI_FUNCTION_TRACE ("LkNamespaceLocateEnd");


    /* We are only interested in opcodes that have an associated name */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    if (!(OpInfo->Flags & AML_NAMED))
    {
        return (AE_OK);
    }

    /* Not interested in name references, we did not open a scope for them */

    if ((Op->Asl.ParseOpcode == PARSEOP_NAMESTRING) ||
        (Op->Asl.ParseOpcode == PARSEOP_NAMESEG)    ||
        (Op->Asl.ParseOpcode == PARSEOP_METHODCALL))
    {
        return (AE_OK);
    }

    /* Pop the scope stack if necessary */

    if (AcpiNsOpensScope (AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode)))
    {

        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "%s: Popping scope for Op %p\n",
            AcpiUtGetTypeName (OpInfo->ObjectType), Op));

        (void) AcpiDsScopeStackPop (WalkState);
    }

    return (AE_OK);
}


