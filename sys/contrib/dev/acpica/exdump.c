/******************************************************************************
 *
 * Module Name: exdump - Interpreter debug output routines
 *              $Revision: 1.188 $
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

#define __EXDUMP_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acinterp.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acparser.h>

#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exdump")

/*
 * The following routines are used for debug output only
 */
#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)

/* Local prototypes */

static void
AcpiExOutString (
    char                    *Title,
    char                    *Value);

static void
AcpiExOutPointer (
    char                    *Title,
    void                    *Value);

static void
AcpiExOutInteger (
    char                    *Title,
    UINT32                  Value);

static void
AcpiExOutAddress (
    char                    *Title,
    ACPI_PHYSICAL_ADDRESS   Value);

static void
AcpiExDumpReference (
    ACPI_OPERAND_OBJECT     *ObjDesc);

static void
AcpiExDumpPackage (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  Level,
    UINT32                  Index);


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDumpOperand
 *
 * PARAMETERS:  *ObjDesc        - Pointer to entry to be dumped
 *              Depth           - Current nesting depth
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump an operand object
 *
 ******************************************************************************/

void
AcpiExDumpOperand (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  Depth)
{
    UINT32                  Length;
    UINT32                  Index;


    ACPI_FUNCTION_NAME ("ExDumpOperand")


    if (!((ACPI_LV_EXEC & AcpiDbgLevel) && (_COMPONENT & AcpiDbgLayer)))
    {
        return;
    }

    if (!ObjDesc)
    {
        /* This could be a null element of a package */

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Null Object Descriptor\n"));
        return;
    }

    if (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) == ACPI_DESC_TYPE_NAMED)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "%p Namespace Node: ", ObjDesc));
        ACPI_DUMP_ENTRY (ObjDesc, ACPI_LV_EXEC);
        return;
    }

    if (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) != ACPI_DESC_TYPE_OPERAND)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "%p is not a node or operand object: [%s]\n",
            ObjDesc, AcpiUtGetDescriptorName (ObjDesc)));
        ACPI_DUMP_BUFFER (ObjDesc, sizeof (ACPI_OPERAND_OBJECT));
        return;
    }

    /* ObjDesc is a valid object */

    if (Depth > 0)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "%*s[%u] %p ",
            Depth, " ", Depth, ObjDesc));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "%p ", ObjDesc));
    }

    /* Decode object type */

    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_LOCAL_REFERENCE:

        switch (ObjDesc->Reference.Opcode)
        {
        case AML_DEBUG_OP:

            AcpiOsPrintf ("Reference: Debug\n");
            break;


        case AML_NAME_OP:

            ACPI_DUMP_PATHNAME (ObjDesc->Reference.Object,
                "Reference: Name: ", ACPI_LV_INFO, _COMPONENT);
            ACPI_DUMP_ENTRY (ObjDesc->Reference.Object, ACPI_LV_INFO);
            break;


        case AML_INDEX_OP:

            AcpiOsPrintf ("Reference: Index %p\n",
                ObjDesc->Reference.Object);
            break;


        case AML_REF_OF_OP:

            AcpiOsPrintf ("Reference: (RefOf) %p\n",
                ObjDesc->Reference.Object);
            break;


        case AML_ARG_OP:

            AcpiOsPrintf ("Reference: Arg%d",
                ObjDesc->Reference.Offset);

            if (ACPI_GET_OBJECT_TYPE (ObjDesc) == ACPI_TYPE_INTEGER)
            {
                /* Value is an Integer */

                AcpiOsPrintf (" value is [%8.8X%8.8x]",
                    ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
            }

            AcpiOsPrintf ("\n");
            break;


        case AML_LOCAL_OP:

            AcpiOsPrintf ("Reference: Local%d",
                ObjDesc->Reference.Offset);

            if (ACPI_GET_OBJECT_TYPE (ObjDesc) == ACPI_TYPE_INTEGER)
            {

                /* Value is an Integer */

                AcpiOsPrintf (" value is [%8.8X%8.8x]",
                    ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
            }

            AcpiOsPrintf ("\n");
            break;


        case AML_INT_NAMEPATH_OP:

            AcpiOsPrintf ("Reference.Node->Name %X\n",
                ObjDesc->Reference.Node->Name.Integer);
            break;


        default:

            /* Unknown opcode */

            AcpiOsPrintf ("Unknown Reference opcode=%X\n",
                ObjDesc->Reference.Opcode);
            break;

        }
        break;


    case ACPI_TYPE_BUFFER:

        AcpiOsPrintf ("Buffer len %X @ %p\n",
            ObjDesc->Buffer.Length, ObjDesc->Buffer.Pointer);

        Length = ObjDesc->Buffer.Length;
        if (Length > 64)
        {
            Length = 64;
        }

        /* Debug only -- dump the buffer contents */

        if (ObjDesc->Buffer.Pointer)
        {
            AcpiOsPrintf ("Buffer Contents: ");

            for (Index = 0; Index < Length; Index++)
            {
                AcpiOsPrintf (" %02x", ObjDesc->Buffer.Pointer[Index]);
            }
            AcpiOsPrintf ("\n");
        }
        break;


    case ACPI_TYPE_INTEGER:

        AcpiOsPrintf ("Integer %8.8X%8.8X\n",
            ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
        break;


    case ACPI_TYPE_PACKAGE:

        AcpiOsPrintf ("Package [Len %X] ElementArray %p\n",
            ObjDesc->Package.Count, ObjDesc->Package.Elements);

        /*
         * If elements exist, package element pointer is valid,
         * and debug_level exceeds 1, dump package's elements.
         */
        if (ObjDesc->Package.Count &&
            ObjDesc->Package.Elements &&
            AcpiDbgLevel > 1)
        {
            for (Index = 0; Index < ObjDesc->Package.Count; Index++)
            {
                AcpiExDumpOperand (ObjDesc->Package.Elements[Index], Depth+1);
            }
        }
        break;


    case ACPI_TYPE_REGION:

        AcpiOsPrintf ("Region %s (%X)",
            AcpiUtGetRegionName (ObjDesc->Region.SpaceId),
            ObjDesc->Region.SpaceId);

        /*
         * If the address and length have not been evaluated,
         * don't print them.
         */
        if (!(ObjDesc->Region.Flags & AOPOBJ_DATA_VALID))
        {
            AcpiOsPrintf ("\n");
        }
        else
        {
            AcpiOsPrintf (" base %8.8X%8.8X Length %X\n",
                ACPI_FORMAT_UINT64 (ObjDesc->Region.Address),
                ObjDesc->Region.Length);
        }
        break;


    case ACPI_TYPE_STRING:

        AcpiOsPrintf ("String length %X @ %p ",
            ObjDesc->String.Length,
            ObjDesc->String.Pointer);

        AcpiUtPrintString (ObjDesc->String.Pointer, ACPI_UINT8_MAX);
        AcpiOsPrintf ("\n");
        break;


    case ACPI_TYPE_LOCAL_BANK_FIELD:

        AcpiOsPrintf ("BankField\n");
        break;


    case ACPI_TYPE_LOCAL_REGION_FIELD:

        AcpiOsPrintf (
            "RegionField: Bits=%X AccWidth=%X Lock=%X Update=%X at byte=%X bit=%X of below:\n",
            ObjDesc->Field.BitLength,
            ObjDesc->Field.AccessByteWidth,
            ObjDesc->Field.FieldFlags & AML_FIELD_LOCK_RULE_MASK,
            ObjDesc->Field.FieldFlags & AML_FIELD_UPDATE_RULE_MASK,
            ObjDesc->Field.BaseByteOffset,
            ObjDesc->Field.StartFieldBitOffset);

        AcpiExDumpOperand (ObjDesc->Field.RegionObj, Depth+1);
        break;


    case ACPI_TYPE_LOCAL_INDEX_FIELD:

        AcpiOsPrintf ("IndexField\n");
        break;


    case ACPI_TYPE_BUFFER_FIELD:

        AcpiOsPrintf (
            "BufferField: %X bits at byte %X bit %X of\n",
            ObjDesc->BufferField.BitLength,
            ObjDesc->BufferField.BaseByteOffset,
            ObjDesc->BufferField.StartFieldBitOffset);

        if (!ObjDesc->BufferField.BufferObj)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "*NULL*\n"));
        }
        else if (ACPI_GET_OBJECT_TYPE (ObjDesc->BufferField.BufferObj) !=
                    ACPI_TYPE_BUFFER)
        {
            AcpiOsPrintf ("*not a Buffer*\n");
        }
        else
        {
            AcpiExDumpOperand (ObjDesc->BufferField.BufferObj, Depth+1);
        }
        break;


    case ACPI_TYPE_EVENT:

        AcpiOsPrintf ("Event\n");
        break;


    case ACPI_TYPE_METHOD:

        AcpiOsPrintf ("Method(%X) @ %p:%X\n",
            ObjDesc->Method.ParamCount,
            ObjDesc->Method.AmlStart,
            ObjDesc->Method.AmlLength);
        break;


    case ACPI_TYPE_MUTEX:

        AcpiOsPrintf ("Mutex\n");
        break;


    case ACPI_TYPE_DEVICE:

        AcpiOsPrintf ("Device\n");
        break;


    case ACPI_TYPE_POWER:

        AcpiOsPrintf ("Power\n");
        break;


    case ACPI_TYPE_PROCESSOR:

        AcpiOsPrintf ("Processor\n");
        break;


    case ACPI_TYPE_THERMAL:

        AcpiOsPrintf ("Thermal\n");
        break;


    default:
        /* Unknown Type */

        AcpiOsPrintf ("Unknown Type %X\n", ACPI_GET_OBJECT_TYPE (ObjDesc));
        break;
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDumpOperands
 *
 * PARAMETERS:  Operands            - Operand list
 *              InterpreterMode     - Load or Exec
 *              Ident               - Identification
 *              NumLevels           - # of stack entries to dump above line
 *              Note                - Output notation
 *              ModuleName          - Caller's module name
 *              LineNumber          - Caller's invocation line number
 *
 * DESCRIPTION: Dump the object stack
 *
 ******************************************************************************/

void
AcpiExDumpOperands (
    ACPI_OPERAND_OBJECT     **Operands,
    ACPI_INTERPRETER_MODE   InterpreterMode,
    char                    *Ident,
    UINT32                  NumLevels,
    char                    *Note,
    char                    *ModuleName,
    UINT32                  LineNumber)
{
    ACPI_NATIVE_UINT        i;


    ACPI_FUNCTION_NAME ("ExDumpOperands");


    if (!Ident)
    {
        Ident = "?";
    }

    if (!Note)
    {
        Note = "?";
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
        "************* Operand Stack Contents (Opcode [%s], %d Operands)\n",
        Ident, NumLevels));

    if (NumLevels == 0)
    {
        NumLevels = 1;
    }

    /* Dump the operand stack starting at the top */

    for (i = 0; NumLevels > 0; i--, NumLevels--)
    {
        AcpiExDumpOperand (Operands[i], 0);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
        "************* Operand Stack dump from %s(%d), %s\n",
        ModuleName, LineNumber, Note));
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOut* functions
 *
 * PARAMETERS:  Title               - Descriptive text
 *              Value               - Value to be displayed
 *
 * DESCRIPTION: Object dump output formatting functions.  These functions
 *              reduce the number of format strings required and keeps them
 *              all in one place for easy modification.
 *
 ******************************************************************************/

static void
AcpiExOutString (
    char                    *Title,
    char                    *Value)
{
    AcpiOsPrintf ("%20s : %s\n", Title, Value);
}

static void
AcpiExOutPointer (
    char                    *Title,
    void                    *Value)
{
    AcpiOsPrintf ("%20s : %p\n", Title, Value);
}

static void
AcpiExOutInteger (
    char                    *Title,
    UINT32                  Value)
{
    AcpiOsPrintf ("%20s : %.2X\n", Title, Value);
}

static void
AcpiExOutAddress (
    char                    *Title,
    ACPI_PHYSICAL_ADDRESS   Value)
{

#if ACPI_MACHINE_WIDTH == 16
    AcpiOsPrintf ("%20s : %p\n", Title, Value);
#else
    AcpiOsPrintf ("%20s : %8.8X%8.8X\n", Title, ACPI_FORMAT_UINT64 (Value));
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDumpNode
 *
 * PARAMETERS:  *Node               - Descriptor to dump
 *              Flags               - Force display if TRUE
 *
 * DESCRIPTION: Dumps the members of the given.Node
 *
 ******************************************************************************/

void
AcpiExDumpNode (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  Flags)
{

    ACPI_FUNCTION_ENTRY ();


    if (!Flags)
    {
        if (!((ACPI_LV_OBJECTS & AcpiDbgLevel) && (_COMPONENT & AcpiDbgLayer)))
        {
            return;
        }
    }

    AcpiOsPrintf ("%20s : %4.4s\n",       "Name", AcpiUtGetNodeName (Node));
    AcpiExOutString  ("Type",             AcpiUtGetTypeName (Node->Type));
    AcpiExOutInteger ("Flags",            Node->Flags);
    AcpiExOutInteger ("Owner Id",         Node->OwnerId);
    AcpiExOutInteger ("Reference Count",  Node->ReferenceCount);
    AcpiExOutPointer ("Attached Object",  AcpiNsGetAttachedObject (Node));
    AcpiExOutPointer ("ChildList",        Node->Child);
    AcpiExOutPointer ("NextPeer",         Node->Peer);
    AcpiExOutPointer ("Parent",           AcpiNsGetParentNode (Node));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDumpReference
 *
 * PARAMETERS:  Object              - Descriptor to dump
 *
 * DESCRIPTION: Dumps a reference object
 *
 ******************************************************************************/

static void
AcpiExDumpReference (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_BUFFER             RetBuf;
    ACPI_STATUS             Status;


    if (ObjDesc->Reference.Opcode == AML_INT_NAMEPATH_OP)
    {
        AcpiOsPrintf ("Named Object %p ", ObjDesc->Reference.Node);
        RetBuf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
        Status = AcpiNsHandleToPathname (ObjDesc->Reference.Node, &RetBuf);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not convert name to pathname\n");
        }
        else
        {
           AcpiOsPrintf ("%s\n", (char *) RetBuf.Pointer);
           ACPI_MEM_FREE (RetBuf.Pointer);
        }
    }
    else if (ObjDesc->Reference.Object)
    {
        AcpiOsPrintf ("\nReferenced Object: %p\n", ObjDesc->Reference.Object);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDumpPackage
 *
 * PARAMETERS:  Object              - Descriptor to dump
 *              Level               - Indentation Level
 *              Index               - Package index for this object
 *
 * DESCRIPTION: Dumps the elements of the package
 *
 ******************************************************************************/

static void
AcpiExDumpPackage (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  Level,
    UINT32                  Index)
{
    UINT32                  i;


    /* Indentation and index output */

    if (Level > 0)
    {
        for (i = 0; i < Level; i++)
        {
            AcpiOsPrintf ("  ");
        }

        AcpiOsPrintf ("[%.2d] ", Index);
    }

    AcpiOsPrintf ("%p ", ObjDesc);

    /* Null package elements are allowed */

    if (!ObjDesc)
    {
        AcpiOsPrintf ("[Null Object]\n");
        return;
    }

    /* Packages may only contain a few object types */

    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_INTEGER:

        AcpiOsPrintf ("[Integer] = %8.8X%8.8X\n",
                    ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
        break;


    case ACPI_TYPE_STRING:

        AcpiOsPrintf ("[String]  Value: ");
        for (i = 0; i < ObjDesc->String.Length; i++)
        {
            AcpiOsPrintf ("%c", ObjDesc->String.Pointer[i]);
        }
        AcpiOsPrintf ("\n");
        break;


    case ACPI_TYPE_BUFFER:

        AcpiOsPrintf ("[Buffer] Length %.2X = ", ObjDesc->Buffer.Length);
        if (ObjDesc->Buffer.Length)
        {
            AcpiUtDumpBuffer ((UINT8 *) ObjDesc->Buffer.Pointer,
                    ObjDesc->Buffer.Length, DB_DWORD_DISPLAY, _COMPONENT);
        }
        else
        {
            AcpiOsPrintf ("\n");
        }
        break;


    case ACPI_TYPE_PACKAGE:

        AcpiOsPrintf ("[Package] Contains %d Elements:\n",
                ObjDesc->Package.Count);

        for (i = 0; i < ObjDesc->Package.Count; i++)
        {
            AcpiExDumpPackage (ObjDesc->Package.Elements[i], Level+1, i);
        }
        break;


    case ACPI_TYPE_LOCAL_REFERENCE:

        AcpiOsPrintf ("[Object Reference] ");
        AcpiExDumpReference (ObjDesc);
        break;


    default:

        AcpiOsPrintf ("[Unknown Type] %X\n", ACPI_GET_OBJECT_TYPE (ObjDesc));
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDumpObjectDescriptor
 *
 * PARAMETERS:  Object              - Descriptor to dump
 *              Flags               - Force display if TRUE
 *
 * DESCRIPTION: Dumps the members of the object descriptor given.
 *
 ******************************************************************************/

void
AcpiExDumpObjectDescriptor (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  Flags)
{
    ACPI_FUNCTION_TRACE ("ExDumpObjectDescriptor");


    if (!ObjDesc)
    {
        return_VOID;
    }

    if (!Flags)
    {
        if (!((ACPI_LV_OBJECTS & AcpiDbgLevel) && (_COMPONENT & AcpiDbgLayer)))
        {
            return_VOID;
        }
    }

    if (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) == ACPI_DESC_TYPE_NAMED)
    {
        AcpiExDumpNode ((ACPI_NAMESPACE_NODE *) ObjDesc, Flags);
        AcpiOsPrintf ("\nAttached Object (%p):\n",
            ((ACPI_NAMESPACE_NODE *) ObjDesc)->Object);
        AcpiExDumpObjectDescriptor (
            ((ACPI_NAMESPACE_NODE *) ObjDesc)->Object, Flags);
        return_VOID;
    }

    if (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) != ACPI_DESC_TYPE_OPERAND)
    {
        AcpiOsPrintf (
            "ExDumpObjectDescriptor: %p is not an ACPI operand object: [%s]\n",
            ObjDesc, AcpiUtGetDescriptorName (ObjDesc));
        return_VOID;
    }

    /* Common Fields */

    AcpiExOutString  ("Type",               AcpiUtGetObjectTypeName (ObjDesc));
    AcpiExOutInteger ("Reference Count",    ObjDesc->Common.ReferenceCount);
    AcpiExOutInteger ("Flags",              ObjDesc->Common.Flags);

    /* Object-specific Fields */

    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_INTEGER:

        AcpiOsPrintf ("%20s : %8.8X%8.8X\n", "Value",
                ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
        break;


    case ACPI_TYPE_STRING:

        AcpiExOutInteger ("Length",         ObjDesc->String.Length);

        AcpiOsPrintf ("%20s : %p  ", "Pointer", ObjDesc->String.Pointer);
        AcpiUtPrintString (ObjDesc->String.Pointer, ACPI_UINT8_MAX);
        AcpiOsPrintf ("\n");
        break;


    case ACPI_TYPE_BUFFER:

        AcpiExOutInteger ("Length",         ObjDesc->Buffer.Length);
        AcpiExOutPointer ("Pointer",        ObjDesc->Buffer.Pointer);
        ACPI_DUMP_BUFFER (ObjDesc->Buffer.Pointer, ObjDesc->Buffer.Length);
        break;


    case ACPI_TYPE_PACKAGE:

        AcpiExOutInteger ("Flags",          ObjDesc->Package.Flags);
        AcpiExOutInteger ("Elements",       ObjDesc->Package.Count);
        AcpiExOutPointer ("Element List",   ObjDesc->Package.Elements);

        /* Dump the package contents */

        AcpiOsPrintf ("\nPackage Contents:\n");
        AcpiExDumpPackage (ObjDesc, 0, 0);
        break;


    case ACPI_TYPE_DEVICE:

        AcpiExOutPointer ("Handler",        ObjDesc->Device.Handler);
        AcpiExOutPointer ("SystemNotify",   ObjDesc->Device.SystemNotify);
        AcpiExOutPointer ("DeviceNotify",   ObjDesc->Device.DeviceNotify);
        break;


    case ACPI_TYPE_EVENT:

        AcpiExOutPointer ("Semaphore",      ObjDesc->Event.Semaphore);
        break;


    case ACPI_TYPE_METHOD:

        AcpiExOutInteger ("ParamCount",     ObjDesc->Method.ParamCount);
        AcpiExOutInteger ("Concurrency",    ObjDesc->Method.Concurrency);
        AcpiExOutPointer ("Semaphore",      ObjDesc->Method.Semaphore);
        AcpiExOutInteger ("OwnerId",        ObjDesc->Method.OwnerId);
        AcpiExOutInteger ("AmlLength",      ObjDesc->Method.AmlLength);
        AcpiExOutPointer ("AmlStart",       ObjDesc->Method.AmlStart);
        break;


    case ACPI_TYPE_MUTEX:

        AcpiExOutInteger ("SyncLevel",      ObjDesc->Mutex.SyncLevel);
        AcpiExOutPointer ("OwnerThread",    ObjDesc->Mutex.OwnerThread);
        AcpiExOutInteger ("AcquireDepth",   ObjDesc->Mutex.AcquisitionDepth);
        AcpiExOutPointer ("Semaphore",      ObjDesc->Mutex.Semaphore);
        break;


    case ACPI_TYPE_REGION:

        AcpiExOutInteger ("SpaceId",        ObjDesc->Region.SpaceId);
        AcpiExOutInteger ("Flags",          ObjDesc->Region.Flags);
        AcpiExOutAddress ("Address",        ObjDesc->Region.Address);
        AcpiExOutInteger ("Length",         ObjDesc->Region.Length);
        AcpiExOutPointer ("Handler",        ObjDesc->Region.Handler);
        AcpiExOutPointer ("Next",           ObjDesc->Region.Next);
        break;


    case ACPI_TYPE_POWER:

        AcpiExOutInteger ("SystemLevel",    ObjDesc->PowerResource.SystemLevel);
        AcpiExOutInteger ("ResourceOrder",  ObjDesc->PowerResource.ResourceOrder);
        AcpiExOutPointer ("SystemNotify",   ObjDesc->PowerResource.SystemNotify);
        AcpiExOutPointer ("DeviceNotify",   ObjDesc->PowerResource.DeviceNotify);
        break;


    case ACPI_TYPE_PROCESSOR:

        AcpiExOutInteger ("Processor ID",   ObjDesc->Processor.ProcId);
        AcpiExOutInteger ("Length",         ObjDesc->Processor.Length);
        AcpiExOutAddress ("Address",        (ACPI_PHYSICAL_ADDRESS) ObjDesc->Processor.Address);
        AcpiExOutPointer ("SystemNotify",   ObjDesc->Processor.SystemNotify);
        AcpiExOutPointer ("DeviceNotify",   ObjDesc->Processor.DeviceNotify);
        AcpiExOutPointer ("Handler",        ObjDesc->Processor.Handler);
        break;


    case ACPI_TYPE_THERMAL:

        AcpiExOutPointer ("SystemNotify",   ObjDesc->ThermalZone.SystemNotify);
        AcpiExOutPointer ("DeviceNotify",   ObjDesc->ThermalZone.DeviceNotify);
        AcpiExOutPointer ("Handler",        ObjDesc->ThermalZone.Handler);
        break;


    case ACPI_TYPE_BUFFER_FIELD:
    case ACPI_TYPE_LOCAL_REGION_FIELD:
    case ACPI_TYPE_LOCAL_BANK_FIELD:
    case ACPI_TYPE_LOCAL_INDEX_FIELD:

        AcpiExOutInteger ("FieldFlags",     ObjDesc->CommonField.FieldFlags);
        AcpiExOutInteger ("AccessByteWidth",ObjDesc->CommonField.AccessByteWidth);
        AcpiExOutInteger ("BitLength",      ObjDesc->CommonField.BitLength);
        AcpiExOutInteger ("FldBitOffset",   ObjDesc->CommonField.StartFieldBitOffset);
        AcpiExOutInteger ("BaseByteOffset", ObjDesc->CommonField.BaseByteOffset);
        AcpiExOutPointer ("ParentNode",     ObjDesc->CommonField.Node);

        switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
        {
        case ACPI_TYPE_BUFFER_FIELD:
            AcpiExOutPointer ("BufferObj",  ObjDesc->BufferField.BufferObj);
            break;

        case ACPI_TYPE_LOCAL_REGION_FIELD:
            AcpiExOutPointer ("RegionObj",  ObjDesc->Field.RegionObj);
            break;

        case ACPI_TYPE_LOCAL_BANK_FIELD:
            AcpiExOutInteger ("Value",      ObjDesc->BankField.Value);
            AcpiExOutPointer ("RegionObj",  ObjDesc->BankField.RegionObj);
            AcpiExOutPointer ("BankObj",    ObjDesc->BankField.BankObj);
            break;

        case ACPI_TYPE_LOCAL_INDEX_FIELD:
            AcpiExOutInteger ("Value",      ObjDesc->IndexField.Value);
            AcpiExOutPointer ("Index",      ObjDesc->IndexField.IndexObj);
            AcpiExOutPointer ("Data",       ObjDesc->IndexField.DataObj);
            break;

        default:
            /* All object types covered above */
            break;
        }
        break;


    case ACPI_TYPE_LOCAL_REFERENCE:

        AcpiExOutInteger ("TargetType",     ObjDesc->Reference.TargetType);
        AcpiExOutString  ("Opcode",         (AcpiPsGetOpcodeInfo (
                                             ObjDesc->Reference.Opcode))->Name);
        AcpiExOutInteger ("Offset",         ObjDesc->Reference.Offset);
        AcpiExOutPointer ("ObjDesc",        ObjDesc->Reference.Object);
        AcpiExOutPointer ("Node",           ObjDesc->Reference.Node);
        AcpiExOutPointer ("Where",          ObjDesc->Reference.Where);

        AcpiExDumpReference (ObjDesc);
        break;


    case ACPI_TYPE_LOCAL_ADDRESS_HANDLER:

        AcpiExOutInteger ("SpaceId",        ObjDesc->AddressSpace.SpaceId);
        AcpiExOutPointer ("Next",           ObjDesc->AddressSpace.Next);
        AcpiExOutPointer ("RegionList",     ObjDesc->AddressSpace.RegionList);
        AcpiExOutPointer ("Node",           ObjDesc->AddressSpace.Node);
        AcpiExOutPointer ("Context",        ObjDesc->AddressSpace.Context);
        break;


    case ACPI_TYPE_LOCAL_NOTIFY:

        AcpiExOutPointer ("Node",           ObjDesc->Notify.Node);
        AcpiExOutPointer ("Context",        ObjDesc->Notify.Context);
        break;


    case ACPI_TYPE_LOCAL_ALIAS:
    case ACPI_TYPE_LOCAL_METHOD_ALIAS:
    case ACPI_TYPE_LOCAL_EXTRA:
    case ACPI_TYPE_LOCAL_DATA:
    default:

        AcpiOsPrintf (
            "ExDumpObjectDescriptor: Display not implemented for object type %s\n",
            AcpiUtGetObjectTypeName (ObjDesc));
        break;
    }

    return_VOID;
}

#endif

