/*******************************************************************************
 *
 * Module Name: dmbuffer - AML disassembler, buffer and string support
 *              $Revision: 8 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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


#include "acpi.h"
#include "acdisasm.h"
#include "acparser.h"
#include "amlcode.h"


#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmbuffer")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisasmByteList
 *
 * PARAMETERS:  Level               - Current source code indentation level
 *              ByteData            - Pointer to the byte list
 *              ByteCount           - Length of the byte list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump a list of bytes in Hex format
 *
 ******************************************************************************/

void
AcpiDmDisasmByteList (
    UINT32                  Level,
    UINT8                   *ByteData,
    UINT32                  ByteCount)
{
    UINT32                  i;


    AcpiDmIndent (Level);

    /* Dump the byte list */

    for (i = 0; i < ByteCount; i++)
    {
        AcpiOsPrintf ("0x%2.2X", (UINT32) ByteData[i]);

        /* Add comma if there are more bytes to display */

        if (i < (ByteCount -1))
        {
            AcpiOsPrintf (", ");
        }

        /* New line every 8 bytes */

        if ((((i+1) % 8) == 0) && ((i+1) < ByteCount))
        {
            AcpiOsPrintf ("\n");
            AcpiDmIndent (Level);
        }
    }

    if (Level)
    {
        AcpiOsPrintf ("\n");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmByteList
 *
 * PARAMETERS:  Info            - Parse tree walk info
 *              Op              - Byte list op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump a buffer byte list, handling the various types of buffers.
 *              Buffer type must be already set in the Op DisasmOpcode.
 *
 ******************************************************************************/

void
AcpiDmByteList (
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;


    ByteData = Op->Named.Data;
    ByteCount = Op->Common.Value.Integer32;

    /*
     * The byte list belongs to a buffer, and can be produced by either
     * a ResourceTemplate, Unicode, quoted string, or a plain byte list.
     */
    switch (Op->Common.Parent->Common.DisasmOpcode)
    {
    case ACPI_DASM_RESOURCE:

        AcpiDmResourceDescriptor (Info, ByteData, ByteCount);
        break;

    case ACPI_DASM_STRING:

        AcpiDmIndent (Info->Level);
        AcpiUtPrintString ((char *) ByteData, ACPI_UINT8_MAX);
        AcpiOsPrintf ("\n");
        break;

    case ACPI_DASM_UNICODE:

        AcpiDmUnicode (Op);
        break;

    case ACPI_DASM_BUFFER:
    default:

        /*
         * Not a resource, string, or unicode string.
         * Just dump the buffer
         */
        AcpiDmDisasmByteList (Info->Level, ByteData, ByteCount);
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsUnicodeBuffer
 *
 * PARAMETERS:  Op              - Buffer Object to be examined
 *
 * RETURN:      TRUE if buffer contains a UNICODE string
 *
 * DESCRIPTION: Determine if a buffer Op contains a Unicode string
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsUnicodeBuffer (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;
    UINT32                  WordCount;
    ACPI_PARSE_OBJECT       *SizeOp;
    ACPI_PARSE_OBJECT       *NextOp;
    NATIVE_UINT             i;


    /* Buffer size is the buffer argument */

    SizeOp = Op->Common.Value.Arg;

    /* Next, the initializer byte list to examine */

    NextOp = SizeOp->Common.Next;
    if (!NextOp)
    {
        return (FALSE);
    }

    /* Extract the byte list info */

    ByteData = NextOp->Named.Data;
    ByteCount = NextOp->Common.Value.Integer32;
    WordCount = ACPI_DIV_2 (ByteCount);

    /*
     * Unicode string must have an even number of bytes and last
     * word must be zero
     */
    if ((!ByteCount)     ||
         (ByteCount < 4) ||
         (ByteCount & 1) ||
        ((UINT16 *) (void *) ByteData)[WordCount - 1] != 0)
    {
        return (FALSE);
    }

    /* For each word, 1st byte must be ascii, 2nd byte must be zero */

    for (i = 0; i < (ByteCount - 2); i += 2)
    {
        if ((!ACPI_IS_PRINT (ByteData[i])) ||
            (ByteData[i + 1] != 0))
        {
            return (FALSE);
        }
    }

    /* Ignore the Size argument in the disassembly of this buffer op */

    SizeOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsStringBuffer
 *
 * PARAMETERS:  Op              - Buffer Object to be examined
 *
 * RETURN:      TRUE if buffer contains a ASCII string
 *
 * DESCRIPTION: Determine if a buffer Op contains a ASCII string
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsStringBuffer (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;
    ACPI_PARSE_OBJECT       *SizeOp;
    ACPI_PARSE_OBJECT       *NextOp;
    UINT32                  i;


    /* Buffer size is the buffer argument */

    SizeOp = Op->Common.Value.Arg;

    /* Next, the initializer byte list to examine */

    NextOp = SizeOp->Common.Next;
    if (!NextOp)
    {
        return (FALSE);
    }

    /* Extract the byte list info */

    ByteData = NextOp->Named.Data;
    ByteCount = NextOp->Common.Value.Integer32;

    /* Last byte must be the null terminator */

    if ((!ByteCount)     ||
         (ByteCount < 2) ||
         (ByteData[ByteCount-1] != 0))
    {
        return (FALSE);
    }

    for (i = 0; i < (ByteCount - 1); i++)
    {
        /* TBD: allow some escapes (non-ascii chars).
         * they will be handled in the string output routine
         */

        if (!ACPI_IS_PRINT (ByteData[i]))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmUnicode
 *
 * PARAMETERS:  Op              - Byte List op containing Unicode string
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump Unicode string as a standard ASCII string.  (Remove
 *              the extra zero bytes).
 *
 ******************************************************************************/

void
AcpiDmUnicode (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT16                  *WordData;
    UINT32                  WordCount;
    UINT32                  i;


    /* Extract the buffer info as a WORD buffer */

    WordData = ACPI_CAST_PTR (UINT16, Op->Named.Data);
    WordCount = ACPI_DIV_2 (Op->Common.Value.Integer32);


    AcpiOsPrintf ("\"");

    /* Write every other byte as an ASCII character */

    for (i = 0; i < (WordCount - 1); i++)
    {
        AcpiOsPrintf ("%c", (int) WordData[i]);
    }

    AcpiOsPrintf ("\")");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiIsEisaId
 *
 * PARAMETERS:  Op              - Op to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Determine if an Op can be converted to an EisaId.
 *
 ******************************************************************************/

void
AcpiIsEisaId (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  Name;
    UINT32                  BigEndianId;
    ACPI_PARSE_OBJECT       *NextOp;
    NATIVE_UINT             i;
    UINT32                  Prefix[3];


    /* Get the NameSegment */

    Name = AcpiPsGetName (Op);
    if (!Name)
    {
        return;
    }

    /* We are looking for _HID */

    if (ACPI_STRNCMP ((char *) &Name, "_HID", 4))
    {
        return;
    }

    /* The parameter must be either a word or a dword */

    NextOp = AcpiPsGetDepthNext (NULL, Op);
    if ((NextOp->Common.AmlOpcode != AML_DWORD_OP) &&
        (NextOp->Common.AmlOpcode != AML_WORD_OP))
    {
        return;
    }

    /* Swap from little-endian to big-endian to simplify conversion */

    BigEndianId = AcpiUtDwordByteSwap (NextOp->Common.Value.Integer32);

    /* Create the 3 leading ASCII letters */

    Prefix[0] = ((BigEndianId >> 26) & 0x1F) + 0x40;
    Prefix[1] = ((BigEndianId >> 21) & 0x1F) + 0x40;
    Prefix[2] = ((BigEndianId >> 16) & 0x1F) + 0x40;

    /* Verify that all 3 are ascii and alpha */

    for (i = 0; i < 3; i++)
    {
        if (!ACPI_IS_ASCII (Prefix[i]) ||
            !ACPI_IS_ALPHA (Prefix[i]))
        {
            return;
        }
    }

    /* OK - mark this node as convertable to an EISA ID */

    NextOp->Common.DisasmOpcode = ACPI_DASM_EISAID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmEisaId
 *
 * PARAMETERS:  EncodedId       - Raw encoded EISA ID.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert an encoded EISAID back to the original ASCII String.
 *
 ******************************************************************************/

void
AcpiDmEisaId (
    UINT32                  EncodedId)
{
    UINT32                  BigEndianId;


    /* Swap from little-endian to big-endian to simplify conversion */

    BigEndianId = AcpiUtDwordByteSwap (EncodedId);


    /* Split to form "AAANNNN" string */

    AcpiOsPrintf ("EisaId (\"%c%c%c%4.4X\")",

        /* Three Alpha characters (AAA), 5 bits each */

        (int) ((BigEndianId >> 26) & 0x1F) + 0x40,
        (int) ((BigEndianId >> 21) & 0x1F) + 0x40,
        (int) ((BigEndianId >> 16) & 0x1F) + 0x40,

        /* Numeric part (NNNN) is simply the lower 16 bits */

        (UINT32) (BigEndianId & 0xFFFF));
}

#endif
