/******************************************************************************
 *
 * Module Name: exdump - Interpreter debug output routines
 *              $Revision: 114 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "actables.h"

#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exdump")


/*
 * The following routines are used for debug output only
 */

#if defined(ACPI_DEBUG) || defined(ENABLE_DEBUGGER)

/*****************************************************************************
 *
 * FUNCTION:    AcpiExShowHexValue
 *
 * PARAMETERS:  ByteCount           - Number of bytes to print (1, 2, or 4)
 *              *AmlPtr             - Address in AML stream of bytes to print
 *              InterpreterMode     - Current running mode (load1/Load2/Exec)
 *              LeadSpace           - # of spaces to print ahead of value
 *                                    0 => none ahead but one behind
 *
 * DESCRIPTION: Print ByteCount byte(s) starting at AmlPtr as a single
 *              value, in hex.  If ByteCount > 1 or the value printed is > 9, also
 *              print in decimal.
 *
 ****************************************************************************/

void
AcpiExShowHexValue (
    UINT32                  ByteCount,
    UINT8                   *AmlPtr,
    UINT32                  LeadSpace)
{
    UINT32                  Value;                  /*  Value retrieved from AML stream */
    UINT32                  ShowDecimalValue;
    UINT32                  Length;                 /*  Length of printed field */
    UINT8                   *CurrentAmlPtr = NULL;  /*  Pointer to current byte of AML value    */


    FUNCTION_TRACE ("ExShowHexValue");


    if (!AmlPtr)
    {
        REPORT_ERROR (("ExShowHexValue: null pointer\n"));
    }

    /*
     * AML numbers are always stored little-endian,
     * even if the processor is big-endian.
     */
    for (CurrentAmlPtr = AmlPtr + ByteCount,
            Value = 0;
            CurrentAmlPtr > AmlPtr; )
    {
        Value = (Value << 8) + (UINT32)* --CurrentAmlPtr;
    }

    Length = LeadSpace * ByteCount + 2;
    if (ByteCount > 1)
    {
        Length += (ByteCount - 1);
    }

    ShowDecimalValue = (ByteCount > 1 || Value > 9);
    if (ShowDecimalValue)
    {
        Length += 3 + AcpiExDigitsNeeded (Value, 10);
    }

    DEBUG_PRINT (TRACE_LOAD, (""));

    for (Length = LeadSpace; Length; --Length )
    {
        DEBUG_PRINT_RAW (TRACE_LOAD, (" "));
    }

    while (ByteCount--)
    {
        DEBUG_PRINT_RAW (TRACE_LOAD, ("%02x", *AmlPtr++));

        if (ByteCount)
        {
            DEBUG_PRINT_RAW (TRACE_LOAD, (" "));
        }
    }

    if (ShowDecimalValue)
    {
        DEBUG_PRINT_RAW (TRACE_LOAD, (" [%ld]", Value));
    }

    if (0 == LeadSpace)
    {
        DEBUG_PRINT_RAW (TRACE_LOAD, (" "));
    }

    DEBUG_PRINT_RAW (TRACE_LOAD, ("\n"));
    return_VOID;
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExDumpOperand
 *
 * PARAMETERS:  *EntryDesc          - Pointer to entry to be dumped
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Dump a stack entry
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExDumpOperand (
    ACPI_OPERAND_OBJECT     *EntryDesc)
{
    UINT8                   *Buf = NULL;
    UINT32                  Length;
    UINT32                  i;


    PROC_NAME ("ExDumpOperand")


    if (!EntryDesc)
    {
        /*
         * This usually indicates that something serious is wrong --
         * since most (if not all)
         * code that dumps the stack expects something to be there!
         */
        DEBUG_PRINTP (ACPI_INFO, ("Null stack entry ptr\n"));
        return (AE_OK);
    }

    if (VALID_DESCRIPTOR_TYPE (EntryDesc, ACPI_DESC_TYPE_NAMED))
    {
        DEBUG_PRINTP (ACPI_INFO, ("Node: \n"));
        DUMP_ENTRY (EntryDesc, ACPI_INFO);
        return (AE_OK);
    }

    if (AcpiTbSystemTablePointer (EntryDesc))
    {
        DEBUG_PRINTP (ACPI_INFO, ("%p is an AML pointer\n",
            EntryDesc));
        return (AE_OK);
    }

    if (!VALID_DESCRIPTOR_TYPE (EntryDesc, ACPI_DESC_TYPE_INTERNAL))
    {
        DEBUG_PRINTP (ACPI_INFO, ("%p Not a local object \n", EntryDesc));
        DUMP_BUFFER (EntryDesc, sizeof (ACPI_OPERAND_OBJECT));
        return (AE_OK);
    }

    /*  EntryDesc is a valid object  */

    DEBUG_PRINTP (ACPI_INFO, ("%p ", EntryDesc));

    switch (EntryDesc->Common.Type)
    {
    case INTERNAL_TYPE_REFERENCE:

        switch (EntryDesc->Reference.Opcode)
        {
        case AML_ZERO_OP:

            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference: Zero\n"));
            break;


        case AML_ONE_OP:

            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference: One\n"));
            break;


        case AML_ONES_OP:

            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference: Ones\n"));
            break;


        case AML_DEBUG_OP:

            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference: Debug\n"));
            break;


        case AML_NAME_OP:

            DUMP_PATHNAME (EntryDesc->Reference.Object, "Reference: Name: ",
                            ACPI_INFO, _COMPONENT);
            DUMP_ENTRY (EntryDesc->Reference.Object, ACPI_INFO);
            break;


        case AML_INDEX_OP:

            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference: Index %p\n",
                        EntryDesc->Reference.Object));
            break;


        case AML_ARG_OP:

            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference: Arg%d",
                        EntryDesc->Reference.Offset));

            if (ACPI_TYPE_INTEGER == EntryDesc->Common.Type)
            {
                /* Value is a Number */

                DEBUG_PRINT_RAW (ACPI_INFO, (" value is [%ld]",
                                            EntryDesc->Integer.Value));
            }

            DEBUG_PRINT_RAW (ACPI_INFO, ("\n"));
            break;


        case AML_LOCAL_OP:

            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference: Local%d",
                        EntryDesc->Reference.Offset));

            if (ACPI_TYPE_INTEGER == EntryDesc->Common.Type)
            {

                /* Value is a Number */

                DEBUG_PRINT_RAW (ACPI_INFO, (" value is [%ld]",
                                            EntryDesc->Integer.Value));
            }

            DEBUG_PRINT_RAW (ACPI_INFO, ("\n"));
            break;


        case AML_INT_NAMEPATH_OP:
            DEBUG_PRINT_RAW (ACPI_INFO, ("Reference.Node->Name %x\n",
                        EntryDesc->Reference.Node->Name));
            break;

        default:

            /*  unknown opcode  */

            DEBUG_PRINT_RAW (ACPI_INFO, ("Unknown opcode=%X\n",
                EntryDesc->Reference.Opcode));
            break;

        }

        break;


    case ACPI_TYPE_BUFFER:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Buffer len %X @ %p \n",
                    EntryDesc->Buffer.Length,
                    EntryDesc->Buffer.Pointer));

        Length = EntryDesc->Buffer.Length;

        if (Length > 64)
        {
            Length = 64;
        }

        /* Debug only -- dump the buffer contents */

        if (EntryDesc->Buffer.Pointer)
        {
            DEBUG_PRINT_RAW (ACPI_INFO, ("Buffer Contents: "));

            for (Buf = EntryDesc->Buffer.Pointer; Length--; ++Buf)
            {
                DEBUG_PRINT_RAW (ACPI_INFO,
                    (Length ? " %02x" : " %02x", *Buf));
            }
            DEBUG_PRINT_RAW (ACPI_INFO,("\n"));
        }

        break;


    case ACPI_TYPE_INTEGER:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Number %lX\n",
                    EntryDesc->Integer.Value));
        break;


    case INTERNAL_TYPE_IF:

        DEBUG_PRINT_RAW (ACPI_INFO, ("If [Number] %lX\n",
                    EntryDesc->Integer.Value));
        break;


    case INTERNAL_TYPE_WHILE:

        DEBUG_PRINT_RAW (ACPI_INFO, ("While [Number] %lX\n",
                    EntryDesc->Integer.Value));
        break;


    case ACPI_TYPE_PACKAGE:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Package count %X @ %p\n",
                    EntryDesc->Package.Count, EntryDesc->Package.Elements));


        /*
         * If elements exist, package vector pointer is valid,
         * and debug_level exceeds 1, dump package's elements.
         */
        if (EntryDesc->Package.Count &&
            EntryDesc->Package.Elements &&
            GetDebugLevel () > 1)
        {
            ACPI_OPERAND_OBJECT**Element;
            UINT16              ElementIndex;

            for (ElementIndex = 0, Element = EntryDesc->Package.Elements;
                  ElementIndex < EntryDesc->Package.Count;
                  ++ElementIndex, ++Element)
            {
                AcpiExDumpOperand (*Element);
            }
        }

        DEBUG_PRINT_RAW (ACPI_INFO, ("\n"));

        break;


    case ACPI_TYPE_REGION:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Region %s (%X)",
            AcpiUtGetRegionName (EntryDesc->Region.SpaceId),
            EntryDesc->Region.SpaceId));

        /*
         * If the address and length have not been evaluated,
         * don't print them.
         */
        if (!(EntryDesc->Region.Flags & AOPOBJ_DATA_VALID))
        {
            DEBUG_PRINT_RAW (ACPI_INFO, ("\n"));
        }
        else
        {
            DEBUG_PRINT_RAW (ACPI_INFO, (" base %p Length %X\n",
                EntryDesc->Region.Address, EntryDesc->Region.Length));
        }
        break;


    case ACPI_TYPE_STRING:

        DEBUG_PRINT_RAW (ACPI_INFO, ("String length %X @ %p\n\n",
                    EntryDesc->String.Length, EntryDesc->String.Pointer));

        for (i=0; i < EntryDesc->String.Length; i++)
        {
            DEBUG_PRINT_RAW (ACPI_INFO, ("%c",
                        EntryDesc->String.Pointer[i]));
        }

        DEBUG_PRINT_RAW (ACPI_INFO, ("\n\n"));
        break;


    case INTERNAL_TYPE_BANK_FIELD:

        DEBUG_PRINT_RAW (ACPI_INFO, ("BankField\n"));
        break;


    case INTERNAL_TYPE_REGION_FIELD:

        DEBUG_PRINT_RAW (ACPI_INFO,
            ("RegionField: bits=%X  bitaccwidth=%X lock=%X update=%X at byte=%lX bit=%X of below:\n",
            EntryDesc->Field.BitLength,      EntryDesc->Field.AccessBitWidth,
            EntryDesc->Field.LockRule,       EntryDesc->Field.UpdateRule,
            EntryDesc->Field.BaseByteOffset, EntryDesc->Field.StartFieldBitOffset));
        DUMP_STACK_ENTRY (EntryDesc->Field.RegionObj);
        break;


    case INTERNAL_TYPE_INDEX_FIELD:

        DEBUG_PRINT_RAW (ACPI_INFO, ("IndexField\n"));
        break;


    case ACPI_TYPE_BUFFER_FIELD:

        DEBUG_PRINT_RAW (ACPI_INFO,
            ("BufferField: %X bits at byte %lX bit %X of \n",
            EntryDesc->BufferField.BitLength, EntryDesc->BufferField.BaseByteOffset,
            EntryDesc->BufferField.StartFieldBitOffset));

        if (!EntryDesc->BufferField.BufferObj)
        {
            DEBUG_PRINT (ACPI_INFO, ("*NULL* \n"));
        }

        else if (ACPI_TYPE_BUFFER !=
                     EntryDesc->BufferField.BufferObj->Common.Type)
        {
            DEBUG_PRINT_RAW (ACPI_INFO, ("*not a Buffer* \n"));
        }

        else
        {
            DUMP_STACK_ENTRY (EntryDesc->BufferField.BufferObj);
        }

        break;


    case ACPI_TYPE_EVENT:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Event\n"));
        break;


    case ACPI_TYPE_METHOD:

        DEBUG_PRINT_RAW (ACPI_INFO,
            ("Method(%X) @ %p:%lX\n",
            EntryDesc->Method.ParamCount,
            EntryDesc->Method.Pcode, EntryDesc->Method.PcodeLength));
        break;


    case ACPI_TYPE_MUTEX:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Mutex\n"));
        break;


    case ACPI_TYPE_DEVICE:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Device\n"));
        break;


    case ACPI_TYPE_POWER:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Power\n"));
        break;


    case ACPI_TYPE_PROCESSOR:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Processor\n"));
        break;


    case ACPI_TYPE_THERMAL:

        DEBUG_PRINT_RAW (ACPI_INFO, ("Thermal\n"));
        break;


    default:
        /*  unknown EntryDesc->Common.Type value    */

        DEBUG_PRINT_RAW (ACPI_INFO, ("Unknown Type %X\n",
            EntryDesc->Common.Type));

        /* Back up to previous entry */

        EntryDesc--;


        /* TBD: [Restructure]  Change to use dump object routine !! */
        /*       What is all of this?? */

        DUMP_BUFFER (EntryDesc, sizeof (ACPI_OPERAND_OBJECT));
        DUMP_BUFFER (++EntryDesc, sizeof (ACPI_OPERAND_OBJECT));
        DUMP_BUFFER (++EntryDesc, sizeof (ACPI_OPERAND_OBJECT));
        break;

    }

    return (AE_OK);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExDumpOperands
 *
 * PARAMETERS:  InterpreterMode      - Load or Exec
 *              *Ident              - Identification
 *              NumLevels           - # of stack entries to dump above line
 *              *Note               - Output notation
 *
 * DESCRIPTION: Dump the object stack
 *
 ****************************************************************************/

void
AcpiExDumpOperands (
    ACPI_OPERAND_OBJECT     **Operands,
    OPERATING_MODE          InterpreterMode,
    NATIVE_CHAR             *Ident,
    UINT32                  NumLevels,
    NATIVE_CHAR             *Note,
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber)
{
    NATIVE_UINT             i;
    ACPI_OPERAND_OBJECT     **EntryDesc;


    if (!Ident)
    {
        Ident = "?";
    }

    if (!Note)
    {
        Note = "?";
    }


    DEBUG_PRINT (ACPI_INFO,
        ("************* AcpiExDumpOperands  Mode=%X ******************\n",
        InterpreterMode));
    DEBUG_PRINT (ACPI_INFO,
        ("From %12s(%d)  %s: %s\n", ModuleName, LineNumber, Ident, Note));

    if (NumLevels == 0)
    {
        NumLevels = 1;
    }

    /* Dump the stack starting at the top, working down */

    for (i = 0; NumLevels > 0; i--, NumLevels--)
    {
        EntryDesc = &Operands[i];

        if (ACPI_FAILURE (AcpiExDumpOperand (*EntryDesc)))
        {
            break;
        }
    }

    return;
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExDumpNode
 *
 * PARAMETERS:  *Node           - Descriptor to dump
 *              Flags               - Force display
 *
 * DESCRIPTION: Dumps the members of the given.Node
 *
 ****************************************************************************/

void
AcpiExDumpNode (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  Flags)
{

    if (!Flags)
    {
        if (!((TRACE_OBJECTS & AcpiDbgLevel) && (_COMPONENT & AcpiDbgLayer)))
        {
            return;
        }
    }


    AcpiOsPrintf ("%20s : %4.4s\n", "Name",             &Node->Name);
    AcpiOsPrintf ("%20s : %s\n",    "Type",             AcpiUtGetTypeName (Node->Type));
    AcpiOsPrintf ("%20s : %X\n",    "Flags",            Node->Flags);
    AcpiOsPrintf ("%20s : %X\n",    "Owner Id",         Node->OwnerId);
    AcpiOsPrintf ("%20s : %X\n",    "Reference Count",  Node->ReferenceCount);
    AcpiOsPrintf ("%20s : %p\n",    "Attached Object",  Node->Object);
    AcpiOsPrintf ("%20s : %p\n",    "ChildList",        Node->Child);
    AcpiOsPrintf ("%20s : %p\n",    "NextPeer",         Node->Peer);
    AcpiOsPrintf ("%20s : %p\n",    "Parent",           AcpiNsGetParentObject (Node));
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExDumpObjectDescriptor
 *
 * PARAMETERS:  *Object             - Descriptor to dump
 *              Flags               - Force display
 *
 * DESCRIPTION: Dumps the members of the object descriptor given.
 *
 ****************************************************************************/

void
AcpiExDumpObjectDescriptor (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  Flags)
{
    FUNCTION_TRACE ("ExDumpObjectDescriptor");


    if (!Flags)
    {
        if (!((TRACE_OBJECTS & AcpiDbgLevel) && (_COMPONENT & AcpiDbgLayer)))
        {
            return;
        }
    }

    if (!(VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_INTERNAL)))
    {
        AcpiOsPrintf ("%p is not a valid ACPI object\n", ObjDesc);
        return;
    }

    /* Common Fields */

    AcpiOsPrintf ("%20s : %X\n",   "Reference Count", ObjDesc->Common.ReferenceCount);
    AcpiOsPrintf ("%20s : %X\n",   "Flags", ObjDesc->Common.Flags);

    /* Object-specific Fields */

    switch (ObjDesc->Common.Type)
    {
    case ACPI_TYPE_INTEGER:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Number");
        AcpiOsPrintf ("%20s : %X\n", "Value", ObjDesc->Integer.Value);
        break;


    case ACPI_TYPE_STRING:

        AcpiOsPrintf ("%20s : %s\n", "Type", "String");
        AcpiOsPrintf ("%20s : %X\n", "Length", ObjDesc->String.Length);
        AcpiOsPrintf ("%20s : %p\n", "Pointer", ObjDesc->String.Pointer);
        break;


    case ACPI_TYPE_BUFFER:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Buffer");
        AcpiOsPrintf ("%20s : %X\n", "Length", ObjDesc->Buffer.Length);
        AcpiOsPrintf ("%20s : %p\n", "Pointer", ObjDesc->Buffer.Pointer);
        break;


    case ACPI_TYPE_PACKAGE:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Package");
        AcpiOsPrintf ("%20s : %X\n", "Count", ObjDesc->Package.Count);
        AcpiOsPrintf ("%20s : %p\n", "Elements", ObjDesc->Package.Elements);
        AcpiOsPrintf ("%20s : %p\n", "NextElement", ObjDesc->Package.NextElement);
        break;


    case ACPI_TYPE_BUFFER_FIELD:

        AcpiOsPrintf ("%20s : %s\n", "Type", "BufferField");
        AcpiOsPrintf ("%20s : %X\n", "BitLength", ObjDesc->BufferField.BitLength);
        AcpiOsPrintf ("%20s : %X\n", "BitOffset", ObjDesc->BufferField.StartFieldBitOffset);
        AcpiOsPrintf ("%20s : %X\n", "BaseByteOffset",ObjDesc->BufferField.BaseByteOffset);
        AcpiOsPrintf ("%20s : %p\n", "BufferObj", ObjDesc->BufferField.BufferObj);
        break;


    case ACPI_TYPE_DEVICE:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Device");
        AcpiOsPrintf ("%20s : %p\n", "AddrHandler", ObjDesc->Device.AddrHandler);
        AcpiOsPrintf ("%20s : %p\n", "SysHandler", ObjDesc->Device.SysHandler);
        AcpiOsPrintf ("%20s : %p\n", "DrvHandler", ObjDesc->Device.DrvHandler);
        break;

    case ACPI_TYPE_EVENT:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Event");
        AcpiOsPrintf ("%20s : %X\n", "Semaphore", ObjDesc->Event.Semaphore);
        break;


    case ACPI_TYPE_METHOD:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Method");
        AcpiOsPrintf ("%20s : %X\n", "ParamCount", ObjDesc->Method.ParamCount);
        AcpiOsPrintf ("%20s : %X\n", "Concurrency", ObjDesc->Method.Concurrency);
        AcpiOsPrintf ("%20s : %p\n", "Semaphore", ObjDesc->Method.Semaphore);
        AcpiOsPrintf ("%20s : %X\n", "PcodeLength", ObjDesc->Method.PcodeLength);
        AcpiOsPrintf ("%20s : %X\n", "Pcode", ObjDesc->Method.Pcode);
        break;


    case ACPI_TYPE_MUTEX:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Mutex");
        AcpiOsPrintf ("%20s : %X\n", "SyncLevel", ObjDesc->Mutex.SyncLevel);
        AcpiOsPrintf ("%20s : %p\n", "Owner", ObjDesc->Mutex.Owner);
        AcpiOsPrintf ("%20s : %X\n", "AcquisitionDepth", ObjDesc->Mutex.AcquisitionDepth);
        AcpiOsPrintf ("%20s : %p\n", "Semaphore", ObjDesc->Mutex.Semaphore);
        break;


    case ACPI_TYPE_REGION:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Region");
        AcpiOsPrintf ("%20s : %X\n", "SpaceId", ObjDesc->Region.SpaceId);
        AcpiOsPrintf ("%20s : %X\n", "Flags", ObjDesc->Region.Flags);
        AcpiOsPrintf ("%20s : %X\n", "Address", ObjDesc->Region.Address);
        AcpiOsPrintf ("%20s : %X\n", "Length", ObjDesc->Region.Length);
        AcpiOsPrintf ("%20s : %p\n", "AddrHandler", ObjDesc->Region.AddrHandler);
        AcpiOsPrintf ("%20s : %p\n", "Next", ObjDesc->Region.Next);
        break;


    case ACPI_TYPE_POWER:

        AcpiOsPrintf ("%20s : %s\n", "Type", "PowerResource");
        AcpiOsPrintf ("%20s : %X\n", "SystemLevel", ObjDesc->PowerResource.SystemLevel);
        AcpiOsPrintf ("%20s : %X\n", "ResourceOrder", ObjDesc->PowerResource.ResourceOrder);
        AcpiOsPrintf ("%20s : %p\n", "SysHandler", ObjDesc->PowerResource.SysHandler);
        AcpiOsPrintf ("%20s : %p\n", "DrvHandler", ObjDesc->PowerResource.DrvHandler);
        break;


    case ACPI_TYPE_PROCESSOR:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Processor");
        AcpiOsPrintf ("%20s : %X\n", "Processor ID", ObjDesc->Processor.ProcId);
        AcpiOsPrintf ("%20s : %X\n", "Length", ObjDesc->Processor.Length);
        AcpiOsPrintf ("%20s : %X\n", "Address", ObjDesc->Processor.Address);
        AcpiOsPrintf ("%20s : %p\n", "SysHandler", ObjDesc->Processor.SysHandler);
        AcpiOsPrintf ("%20s : %p\n", "DrvHandler", ObjDesc->Processor.DrvHandler);
        AcpiOsPrintf ("%20s : %p\n", "AddrHandler", ObjDesc->Processor.AddrHandler);
        break;


    case ACPI_TYPE_THERMAL:

        AcpiOsPrintf ("%20s : %s\n", "Type", "ThermalZone");
        AcpiOsPrintf ("%20s : %p\n", "SysHandler", ObjDesc->ThermalZone.SysHandler);
        AcpiOsPrintf ("%20s : %p\n", "DrvHandler", ObjDesc->ThermalZone.DrvHandler);
        AcpiOsPrintf ("%20s : %p\n", "AddrHandler", ObjDesc->ThermalZone.AddrHandler);
        break;


    case INTERNAL_TYPE_REGION_FIELD:

        AcpiOsPrintf ("%20s : %p\n", "AccessBitWidth", ObjDesc->Field.AccessBitWidth);
        AcpiOsPrintf ("%20s : %p\n", "BitLength", ObjDesc->Field.BitLength);
        AcpiOsPrintf ("%20s : %p\n", "BaseByteOffset",ObjDesc->Field.BaseByteOffset);
        AcpiOsPrintf ("%20s : %p\n", "BitOffset", ObjDesc->Field.StartFieldBitOffset);
        AcpiOsPrintf ("%20s : %p\n", "RegionObj", ObjDesc->Field.RegionObj);
        break;


    case INTERNAL_TYPE_BANK_FIELD:

        AcpiOsPrintf ("%20s : %s\n", "Type", "BankField");
        AcpiOsPrintf ("%20s : %X\n", "AccessBitWidth", ObjDesc->BankField.AccessBitWidth);
        AcpiOsPrintf ("%20s : %X\n", "LockRule", ObjDesc->BankField.LockRule);
        AcpiOsPrintf ("%20s : %X\n", "UpdateRule", ObjDesc->BankField.UpdateRule);
        AcpiOsPrintf ("%20s : %X\n", "BitLength", ObjDesc->BankField.BitLength);
        AcpiOsPrintf ("%20s : %X\n", "BitOffset", ObjDesc->BankField.StartFieldBitOffset);
        AcpiOsPrintf ("%20s : %X\n", "BaseByteOffset", ObjDesc->BankField.BaseByteOffset);
        AcpiOsPrintf ("%20s : %X\n", "Value", ObjDesc->BankField.Value);
        AcpiOsPrintf ("%20s : %p\n", "RegionObj", ObjDesc->BankField.RegionObj);
        AcpiOsPrintf ("%20s : %X\n", "BankRegisterObj", ObjDesc->BankField.BankRegisterObj);
        break;


    case INTERNAL_TYPE_INDEX_FIELD:

        AcpiOsPrintf ("%20s : %s\n", "Type", "IndexField");
        AcpiOsPrintf ("%20s : %X\n", "AccessBitWidth", ObjDesc->IndexField.AccessBitWidth);
        AcpiOsPrintf ("%20s : %X\n", "LockRule", ObjDesc->IndexField.LockRule);
        AcpiOsPrintf ("%20s : %X\n", "UpdateRule", ObjDesc->IndexField.UpdateRule);
        AcpiOsPrintf ("%20s : %X\n", "BitLength", ObjDesc->IndexField.BitLength);
        AcpiOsPrintf ("%20s : %X\n", "BitOffset", ObjDesc->IndexField.StartFieldBitOffset);
        AcpiOsPrintf ("%20s : %X\n", "Value", ObjDesc->IndexField.Value);
        AcpiOsPrintf ("%20s : %X\n", "Index", ObjDesc->IndexField.IndexObj);
        AcpiOsPrintf ("%20s : %X\n", "Data", ObjDesc->IndexField.DataObj);
        break;


    case INTERNAL_TYPE_REFERENCE:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Reference");
        AcpiOsPrintf ("%20s : %X\n", "TargetType", ObjDesc->Reference.TargetType);
        AcpiOsPrintf ("%20s : %X\n", "Opcode", ObjDesc->Reference.Opcode);
        AcpiOsPrintf ("%20s : %X\n", "Offset", ObjDesc->Reference.Offset);
        AcpiOsPrintf ("%20s : %p\n", "ObjDesc", ObjDesc->Reference.Object);
        AcpiOsPrintf ("%20s : %p\n", "Node", ObjDesc->Reference.Node);
        AcpiOsPrintf ("%20s : %p\n", "Where", ObjDesc->Reference.Where);
        break;


    case INTERNAL_TYPE_ADDRESS_HANDLER:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Address Handler");
        AcpiOsPrintf ("%20s : %X\n", "SpaceId", ObjDesc->AddrHandler.SpaceId);
        AcpiOsPrintf ("%20s : %p\n", "Next", ObjDesc->AddrHandler.Next);
        AcpiOsPrintf ("%20s : %p\n", "RegionList", ObjDesc->AddrHandler.RegionList);
        AcpiOsPrintf ("%20s : %p\n", "Node", ObjDesc->AddrHandler.Node);
        AcpiOsPrintf ("%20s : %p\n", "Handler", ObjDesc->AddrHandler.Handler);
        AcpiOsPrintf ("%20s : %p\n", "Context", ObjDesc->AddrHandler.Context);
        break;


    case INTERNAL_TYPE_NOTIFY:

        AcpiOsPrintf ("%20s : %s\n", "Type", "Notify Handler");
        AcpiOsPrintf ("%20s : %p\n", "Node", ObjDesc->NotifyHandler.Node);
        AcpiOsPrintf ("%20s : %p\n", "Handler", ObjDesc->NotifyHandler.Handler);
        AcpiOsPrintf ("%20s : %p\n", "Context", ObjDesc->NotifyHandler.Context);
        break;


    case INTERNAL_TYPE_ALIAS:
    case INTERNAL_TYPE_FIELD_DEFN:
    case INTERNAL_TYPE_BANK_FIELD_DEFN:
    case INTERNAL_TYPE_INDEX_FIELD_DEFN:
    case INTERNAL_TYPE_IF:
    case INTERNAL_TYPE_ELSE:
    case INTERNAL_TYPE_WHILE:
    case INTERNAL_TYPE_SCOPE:
    case INTERNAL_TYPE_DEF_ANY:

        AcpiOsPrintf ("*** Structure display not implemented for type %X! ***\n",
            ObjDesc->Common.Type);
        break;


    default:

        AcpiOsPrintf ("*** Cannot display unknown type %X! ***\n", ObjDesc->Common.Type);
        break;
    }

    return_VOID;
}

#endif

