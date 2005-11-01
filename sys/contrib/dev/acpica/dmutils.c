/*******************************************************************************
 *
 * Module Name: dmutils - AML disassembler utilities
 *              $Revision: 1.19 $
 *
 ******************************************************************************/

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


#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acdisasm.h>

#ifdef ACPI_ASL_COMPILER
#include <contrib/dev/acpica/acnamesp.h>
#endif

#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmutils")


ACPI_EXTERNAL_LIST              *AcpiGbl_ExternalList = NULL;


/* Data used in keeping track of fields */
#if 0
const char                      *AcpiGbl_FENames[ACPI_NUM_FIELD_NAMES] =
{
    "skip",
    "?access?"
};              /* FE = Field Element */
#endif


const char                      *AcpiGbl_MatchOps[ACPI_NUM_MATCH_OPS] =
{
    "MTR",
    "MEQ",
    "MLE",
    "MLT",
    "MGE",
    "MGT"
};


/* Access type decoding */

const char                      *AcpiGbl_AccessTypes[ACPI_NUM_ACCESS_TYPES] =
{
    "AnyAcc",
    "ByteAcc",
    "WordAcc",
    "DWordAcc",
    "QWordAcc",
    "BufferAcc",
};


/* Lock rule decoding */

const char                      *AcpiGbl_LockRule[ACPI_NUM_LOCK_RULES] =
{
    "NoLock",
    "Lock"
};

/* Update rule decoding */

const char                      *AcpiGbl_UpdateRules[ACPI_NUM_UPDATE_RULES] =
{
    "Preserve",
    "WriteAsOnes",
    "WriteAsZeros"
};

/*
 * Strings used to decode resource descriptors
 */
const char                      *AcpiGbl_IoDecode[2] =
{
    "Decode10",
    "Decode16"
};

const char                      *AcpiGbl_WordDecode[4] =
{
    "Memory",
    "IO",
    "BusNumber",
    "Unknown-resource-type"
};

const char                      *AcpiGbl_ConsumeDecode[2] =
{
    "ResourceProducer",
    "ResourceConsumer"
};

const char                      *AcpiGbl_MinDecode[2] =
{
    "MinNotFixed",
    "MinFixed"
};

const char                      *AcpiGbl_MaxDecode[2] =
{
    "MaxNotFixed",
    "MaxFixed"
};

const char                      *AcpiGbl_DECDecode[2] =
{
    "PosDecode",
    "SubDecode"
};

const char                      *AcpiGbl_IrqDecode[2] =
{
    "IRQNoFlags",
    "IRQ"
};

const char                      *AcpiGbl_HEDecode[2] =
{
    "Level",
    "Edge"
};

const char                      *AcpiGbl_LLDecode[2] =
{
    "ActiveHigh",
    "ActiveLow"
};

const char                      *AcpiGbl_SHRDecode[2] =
{
    "Exclusive",
    "Shared"
};

const char                      *AcpiGbl_TYPDecode[4] =
{
    "Compatibility",
    "TypeA",
    "TypeB",
    "TypeF"
};

const char                      *AcpiGbl_BMDecode[2] =
{
    "NotBusMaster",
    "BusMaster"
};

const char                      *AcpiGbl_SIZDecode[4] =
{
    "Transfer8",
    "Transfer8_16",
    "Transfer16",
    "InvalidSize"
};

/* Type Specific Flags */

const char                      *AcpiGbl_TTPDecode[2] =
{
    "TypeStatic",
    "TypeTranslation"
};

const char                      *AcpiGbl_MTPDecode[4] =
{
    "AddressRangeMemory",
    "AddressRangeReserved",
    "AddressRangeACPI",
    "AddressRangeNVS"
};

const char                      *AcpiGbl_MEMDecode[4] =
{
    "NonCacheable",
    "Cacheable",
    "WriteCombining",
    "Prefetchable"
};

const char                      *AcpiGbl_RWDecode[2] =
{
    "ReadOnly",
    "ReadWrite"
};

const char                      *AcpiGbl_TRSDecode[2] =
{
    "DenseTranslation",
    "SparseTranslation"
};

const char                      *AcpiGbl_RNGDecode[4] =
{
    "InvalidRanges",
    "NonISAOnlyRanges",
    "ISAOnlyRanges",
    "EntireRange"
};

const char                      *AcpiGbl_ConfigDecode[4] =
{
    "0 - Good Configuration",
    "1 - Acceptable Configuration",
    "2 - Suboptimal Configuration",
    "3 - ***Invalid Configuration***",
};


#ifdef ACPI_ASL_COMPILER
/*******************************************************************************
 *
 * FUNCTION:    AcpiDmAddToExternalList
 *
 * PARAMETERS:  Path            - Internal (AML) path to the object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert a new path into the list of Externals which will in
 *              turn be emitted as an External() declaration in the disassembled
 *              output.
 *
 ******************************************************************************/

void
AcpiDmAddToExternalList (
    char                    *Path)
{
    char                    *ExternalPath;
    ACPI_EXTERNAL_LIST      *NewExternal;
    ACPI_STATUS             Status;


    if (!Path)
    {
        return;
    }

    /* Externalize the ACPI path */

    Status = AcpiNsExternalizeName (ACPI_UINT32_MAX, Path,
                    NULL, &ExternalPath);
    if (ACPI_SUCCESS (Status))
    {
        /* Allocate and init a new External() descriptor */

        NewExternal = ACPI_MEM_CALLOCATE (sizeof (ACPI_EXTERNAL_LIST));
        NewExternal->Path = ExternalPath;

        /* Link the new descriptor into the global list */

        if (AcpiGbl_ExternalList)
        {
            NewExternal->Next = AcpiGbl_ExternalList;
        }
        AcpiGbl_ExternalList = NewExternal;
    }
}
#endif

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDecodeAttribute
 *
 * PARAMETERS:  Attribute       - Attribute field of AccessAs keyword
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode the AccessAs attribute byte.  (Mostly SMBus stuff)
 *
 ******************************************************************************/

void
AcpiDmDecodeAttribute (
    UINT8                   Attribute)
{

    switch (Attribute)
    {
    case AML_FIELD_ATTRIB_SMB_QUICK:

        AcpiOsPrintf ("SMBQuick");
        break;

    case AML_FIELD_ATTRIB_SMB_SEND_RCV:

        AcpiOsPrintf ("SMBSendReceive");
        break;

    case AML_FIELD_ATTRIB_SMB_BYTE:

        AcpiOsPrintf ("SMBByte");
        break;

    case AML_FIELD_ATTRIB_SMB_WORD:

        AcpiOsPrintf ("SMBWord");
        break;

    case AML_FIELD_ATTRIB_SMB_WORD_CALL:

        AcpiOsPrintf ("SMBProcessCall");
        break;

    case AML_FIELD_ATTRIB_SMB_BLOCK:

        AcpiOsPrintf ("SMBBlock");
        break;

    case AML_FIELD_ATTRIB_SMB_BLOCK_CALL:

        AcpiOsPrintf ("SMBBlockProcessCall");
        break;

    default:

        AcpiOsPrintf ("0x%.2X", Attribute);
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIndent
 *
 * PARAMETERS:  Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Indent 4 spaces per indentation level.
 *
 ******************************************************************************/

void
AcpiDmIndent (
    UINT32                  Level)
{

    if (!Level)
    {
        return;
    }

    AcpiOsPrintf ("%*.s", ACPI_MUL_4 (Level), " ");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommaIfListMember
 *
 * PARAMETERS:  Op              - Current operator/operand
 *
 * RETURN:      TRUE if a comma was inserted
 *
 * DESCRIPTION: Insert a comma if this Op is a member of an argument list.
 *
 ******************************************************************************/

BOOLEAN
AcpiDmCommaIfListMember (
    ACPI_PARSE_OBJECT       *Op)
{

    if (!Op->Common.Next)
    {
        return FALSE;
    }

    if (AcpiDmListType (Op->Common.Parent) & BLOCK_COMMA_LIST)
    {
        /* Check for a NULL target operand */

        if ((Op->Common.Next->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
            (!Op->Common.Next->Common.Value.String))
        {
            /*
             * To handle the Divide() case where there are two optional
             * targets, look ahead one more op.  If null, this null target
             * is the one and only target -- no comma needed.  Otherwise,
             * we need a comma to prepare for the next target.
             */
            if (!Op->Common.Next->Common.Next)
            {
                return FALSE;
            }
        }

        if ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST) &&
            (!(Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST)))
        {
            return FALSE;
        }

        AcpiOsPrintf (", ");
        return (TRUE);
    }

    else if ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST) &&
             (Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST))
    {
        AcpiOsPrintf (", ");
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommaIfFieldMember
 *
 * PARAMETERS:  Op              - Current operator/operand
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert a comma if this Op is a member of a Field argument list.
 *
 ******************************************************************************/

void
AcpiDmCommaIfFieldMember (
    ACPI_PARSE_OBJECT       *Op)
{

    if (Op->Common.Next)
    {
        AcpiOsPrintf (", ");
    }
}

#endif
