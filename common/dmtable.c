/******************************************************************************
 *
 * Module Name: dmtable - Support for ACPI tables that contain no AML code
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
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
#include "accommon.h"
#include "acdisasm.h"
#include "actables.h"

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtable")

/* Local Prototypes */

static ACPI_DMTABLE_DATA *
AcpiDmGetTableData (
    char                    *Signature);

static void
AcpiDmCheckAscii (
    UINT8                   *Target,
    char                    *RepairedName,
    UINT32                  Count);

UINT8
AcpiTbGenerateChecksum (
    ACPI_TABLE_HEADER       *Table);


/* These tables map a subtable type to a description string */

static const char           *AcpiDmAsfSubnames[] =
{
    "ASF Information",
    "ASF Alerts",
    "ASF Remote Control",
    "ASF RMCP Boot Options",
    "ASF Address",
    "Unknown SubTable Type"         /* Reserved */
};

static const char           *AcpiDmDmarSubnames[] =
{
    "Hardware Unit Definition",
    "Reserved Memory Region",
    "Root Port ATS Capability",
    "Remapping Hardware Static Affinity",
    "Unknown SubTable Type"         /* Reserved */
};

static const char           *AcpiDmHestSubnames[] =
{
    "IA-32 Machine Check Exception",
    "IA-32 Corrected Machine Check",
    "IA-32 Non-Maskable Interrupt",
    "Unknown SubTable Type",        /* 3 - Reserved */
    "Unknown SubTable Type",        /* 4 - Reserved */
    "Unknown SubTable Type",        /* 5 - Reserved */
    "PCI Express Root Port AER",
    "PCI Express AER (AER Endpoint)",
    "PCI Express/PCI-X Bridge AER",
    "Generic Hardware Error Source",
    "Unknown SubTable Type"         /* Reserved */
};

static const char           *AcpiDmHestNotifySubnames[] =
{
    "Polled",
    "External Interrupt",
    "Local Interrupt",
    "SCI",
    "NMI",
    "Unknown Notify Type"           /* Reserved */
};

static const char           *AcpiDmMadtSubnames[] =
{
    "Processor Local APIC",         /* ACPI_MADT_TYPE_LOCAL_APIC */
    "I/O APIC",                     /* ACPI_MADT_TYPE_IO_APIC */
    "Interrupt Source Override",    /* ACPI_MADT_TYPE_INTERRUPT_OVERRIDE */
    "NMI Source",                   /* ACPI_MADT_TYPE_NMI_SOURCE */
    "Local APIC NMI",               /* ACPI_MADT_TYPE_LOCAL_APIC_NMI */
    "Local APIC Address Override",  /* ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE */
    "I/O SAPIC",                    /* ACPI_MADT_TYPE_IO_SAPIC */
    "Local SAPIC",                  /* ACPI_MADT_TYPE_LOCAL_SAPIC */
    "Platform Interrupt Sources",   /* ACPI_MADT_TYPE_INTERRUPT_SOURCE */
    "Processor Local x2APIC",       /* ACPI_MADT_TYPE_LOCAL_X2APIC */
    "Local x2APIC NMI",             /* ACPI_MADT_TYPE_LOCAL_X2APIC_NMI */
    "Unknown SubTable Type"         /* Reserved */
};

static const char           *AcpiDmSratSubnames[] =
{
    "Processor Local APIC/SAPIC Affinity",
    "Memory Affinity",
    "Processor Local x2APIC Affinity",
    "Unknown SubTable Type"         /* Reserved */
};

static const char           *AcpiDmIvrsSubnames[] =
{
    "Hardware Definition Block",
    "Memory Definition Block",
    "Unknown SubTable Type"         /* Reserved */
};


#define ACPI_FADT_PM_RESERVED       8

static const char           *AcpiDmFadtProfiles[] =
{
    "Unspecified",
    "Desktop",
    "Mobile",
    "Workstation",
    "Enterprise Server",
    "SOHO Server",
    "Appliance PC",
    "Performance Server",
    "Unknown Profile Type"
};

/*******************************************************************************
 *
 * ACPI Table Data, indexed by signature.
 *
 * Each entry contains: Signature, Table Info, Handler, Description
 *
 * Simple tables have only a TableInfo structure, complex tables have a handler.
 * This table must be NULL terminated. RSDP and FACS are special-cased
 * elsewhere.
 *
 ******************************************************************************/

static ACPI_DMTABLE_DATA    AcpiDmTableData[] =
{
    {ACPI_SIG_ASF,  NULL,                   AcpiDmDumpAsf,  "Alert Standard Format table"},
    {ACPI_SIG_BOOT, AcpiDmTableInfoBoot,    NULL,           "Simple Boot Flag Table"},
    {ACPI_SIG_BERT, AcpiDmTableInfoBert,    NULL,           "Boot Error Record Table"},
    {ACPI_SIG_CPEP, NULL,                   AcpiDmDumpCpep, "Corrected Platform Error Polling table"},
    {ACPI_SIG_DBGP, AcpiDmTableInfoDbgp,    NULL,           "Debug Port table"},
    {ACPI_SIG_DMAR, NULL,                   AcpiDmDumpDmar, "DMA Remapping table"},
    {ACPI_SIG_ECDT, AcpiDmTableInfoEcdt,    NULL,           "Embedded Controller Boot Resources Table"},
    {ACPI_SIG_EINJ, NULL,                   AcpiDmDumpEinj, "Error Injection table"},
    {ACPI_SIG_ERST, NULL,                   AcpiDmDumpErst, "Error Record Serialization Table"},
    {ACPI_SIG_FADT, NULL,                   AcpiDmDumpFadt, "Fixed ACPI Description Table"},
    {ACPI_SIG_HEST, NULL,                   AcpiDmDumpHest, "Hardware Error Source Table"},
    {ACPI_SIG_HPET, AcpiDmTableInfoHpet,    NULL,           "High Precision Event Timer table"},
    {ACPI_SIG_IVRS, NULL,                   AcpiDmDumpIvrs, "I/O Virtualization Reporting Structure"},
    {ACPI_SIG_MADT, NULL,                   AcpiDmDumpMadt, "Multiple APIC Description Table"},
    {ACPI_SIG_MCFG, NULL,                   AcpiDmDumpMcfg, "Memory Mapped Configuration table"},
    {ACPI_SIG_MSCT, NULL,                   AcpiDmDumpMsct, "Maximum System Characteristics Table"},
    {ACPI_SIG_RSDT, NULL,                   AcpiDmDumpRsdt, "Root System Description Table"},
    {ACPI_SIG_SBST, AcpiDmTableInfoSbst,    NULL,           "Smart Battery Specification Table"},
    {ACPI_SIG_SLIC, AcpiDmTableInfoSlic,    NULL,           "Software Licensing Description Table"},
    {ACPI_SIG_SLIT, NULL,                   AcpiDmDumpSlit, "System Locality Information Table"},
    {ACPI_SIG_SPCR, AcpiDmTableInfoSpcr,    NULL,           "Serial Port Console Redirection table"},
    {ACPI_SIG_SPMI, AcpiDmTableInfoSpmi,    NULL,           "Server Platform Management Interface table"},
    {ACPI_SIG_SRAT, NULL,                   AcpiDmDumpSrat, "System Resource Affinity Table"},
    {ACPI_SIG_TCPA, AcpiDmTableInfoTcpa,    NULL,           "Trusted Computing Platform Alliance table"},
    {ACPI_SIG_UEFI, AcpiDmTableInfoUefi,    NULL,           "UEFI Boot Optimization Table"},
    {ACPI_SIG_WAET, AcpiDmTableInfoWaet,    NULL,           "Windows ACPI Emulated Devices Table"},
    {ACPI_SIG_WDAT, NULL,                   AcpiDmDumpWdat, "Watchdog Action Table"},
    {ACPI_SIG_WDRT, AcpiDmTableInfoWdrt,    NULL,           "Watchdog Resource Table"},
    {ACPI_SIG_XSDT, NULL,                   AcpiDmDumpXsdt, "Extended System Description Table"},
    {NULL,          NULL,                   NULL,           NULL}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGenerateChecksum
 *
 * PARAMETERS:  Table               - Pointer to a valid ACPI table (with a
 *                                    standard ACPI header)
 *
 * RETURN:      8 bit checksum of buffer
 *
 * DESCRIPTION: Computes an 8 bit checksum of the table.
 *
 ******************************************************************************/

UINT8
AcpiTbGenerateChecksum (
    ACPI_TABLE_HEADER       *Table)
{
    UINT8                   Checksum;


    /* Sum the entire table as-is */

    Checksum = AcpiTbChecksum ((UINT8 *) Table, Table->Length);

    /* Subtract off the existing checksum value in the table */

    Checksum = (UINT8) (Checksum - Table->Checksum);

    /* Compute the final checksum */

    Checksum = (UINT8) (0 - Checksum);
    return (Checksum);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmGetTableData
 *
 * PARAMETERS:  Signature           - ACPI signature (4 chars) to match
 *
 * RETURN:      Pointer to a valid ACPI_DMTABLE_DATA. Null if no match found.
 *
 * DESCRIPTION: Find a match in the global table of supported ACPI tables
 *
 ******************************************************************************/

static ACPI_DMTABLE_DATA *
AcpiDmGetTableData (
    char                    *Signature)
{
    ACPI_DMTABLE_DATA       *TableData;


    for (TableData = AcpiDmTableData; TableData->Signature; TableData++)
    {
        if (ACPI_COMPARE_NAME (Signature, TableData->Signature))
        {
            return (TableData);
        }
    }

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDataTable
 *
 * PARAMETERS:  Table               - An ACPI table
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Format the contents of an ACPI data table (any table other
 *              than an SSDT or DSDT that does not contain executable AML code)
 *
 ******************************************************************************/

void
AcpiDmDumpDataTable (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_DMTABLE_DATA       *TableData;
    UINT32                  Length;


    /* Ignore tables that contain AML */

    if (AcpiUtIsAmlTable (Table))
    {
        return;
    }

    /*
     * Handle tables that don't use the common ACPI table header structure.
     * Currently, these are the FACS and RSDP.
     */
    if (ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_FACS))
    {
        Length = Table->Length;
        AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoFacs);
    }
    else if (ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_RSDP))
    {
        Length = AcpiDmDumpRsdp (Table);
    }
    else
    {
        /*
         * All other tables must use the common ACPI table header, dump it now
         */
        Length = Table->Length;
        Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoHeader);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
        AcpiOsPrintf ("\n");

        /* Match signature and dispatch appropriately */

        TableData = AcpiDmGetTableData (Table->Signature);
        if (!TableData)
        {
            if (!ACPI_STRNCMP (Table->Signature, "OEM", 3))
            {
                AcpiOsPrintf ("\n**** OEM-defined ACPI table [%4.4s], unknown contents\n\n",
                    Table->Signature);
            }
            else
            {
                AcpiOsPrintf ("\n**** Unknown ACPI table type [%4.4s]\n\n",
                    Table->Signature);
            }
        }
        else if (TableData->TableHandler)
        {
            /* Complex table, has a handler */

            TableData->TableHandler (Table);
        }
        else if (TableData->TableInfo)
        {
            /* Simple table, just walk the info table */

            AcpiDmDumpTable (Length, 0, Table, 0, TableData->TableInfo);
        }
    }

    /* Always dump the raw table data */

    AcpiOsPrintf ("\nRaw Table Data\n\n");
    AcpiUtDumpBuffer2 (ACPI_CAST_PTR (UINT8, Table), Length, DB_BYTE_DISPLAY);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmLineHeader
 *
 * PARAMETERS:  Offset              - Current byte offset, from table start
 *              ByteLength          - Length of the field in bytes, 0 for flags
 *              Name                - Name of this field
 *              Value               - Optional value, displayed on left of ':'
 *
 * RETURN:      None
 *
 * DESCRIPTION: Utility routines for formatting output lines. Displays the
 *              current table offset in hex and decimal, the field length,
 *              and the field name.
 *
 ******************************************************************************/

void
AcpiDmLineHeader (
    UINT32                  Offset,
    UINT32                  ByteLength,
    char                    *Name)
{

    if (ByteLength)
    {
        AcpiOsPrintf ("[%3.3Xh %4.4d% 3d] %28s : ",
            Offset, Offset, ByteLength, Name);
    }
    else
    {
        AcpiOsPrintf ("%43s : ",
            Name);
    }
}

void
AcpiDmLineHeader2 (
    UINT32                  Offset,
    UINT32                  ByteLength,
    char                    *Name,
    UINT32                  Value)
{

    if (ByteLength)
    {
        AcpiOsPrintf ("[%3.3Xh %4.4d% 3d] %24s % 3d : ",
            Offset, Offset, ByteLength, Name, Value);
    }
    else
    {
        AcpiOsPrintf ("[%3.3Xh %4.4d   ] %24s % 3d : ",
            Offset, Offset, Name, Value);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpTable
 *
 * PARAMETERS:  TableLength         - Length of the entire ACPI table
 *              TableOffset         - Starting offset within the table for this
 *                                    sub-descriptor (0 if main table)
 *              Table               - The ACPI table
 *              SubtableLength      - Length of this sub-descriptor
 *              Info                - Info table for this ACPI table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display ACPI table contents by walking the Info table.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDmDumpTable (
    UINT32                  TableLength,
    UINT32                  TableOffset,
    void                    *Table,
    UINT32                  SubtableLength,
    ACPI_DMTABLE_INFO       *Info)
{
    UINT8                   *Target;
    UINT32                  CurrentOffset;
    UINT32                  ByteLength;
    UINT8                   Temp8;
    UINT16                  Temp16;
    ACPI_DMTABLE_DATA       *TableData;
    const char              *Name;
    BOOLEAN                 LastOutputBlankLine = FALSE;
    char                    RepairedName[8];


    if (!Info)
    {
        AcpiOsPrintf ("Display not implemented\n");
        return (AE_NOT_IMPLEMENTED);
    }

    /* Walk entire Info table; Null name terminates */

    for (; Info->Name; Info++)
    {
        /*
         * Target points to the field within the ACPI Table. CurrentOffset is
         * the offset of the field from the start of the main table.
         */
        Target = ACPI_ADD_PTR (UINT8, Table, Info->Offset);
        CurrentOffset = TableOffset + Info->Offset;

        /* Check for beyond EOT or beyond subtable end */

        if ((CurrentOffset >= TableLength) ||
            (SubtableLength && (Info->Offset >= SubtableLength)))
        {
            AcpiOsPrintf ("**** ACPI table terminates in the middle of a data structure!\n");
            return (AE_BAD_DATA);
        }

        /* Generate the byte length for this field */

        switch (Info->Opcode)
        {
        case ACPI_DMT_UINT8:
        case ACPI_DMT_CHKSUM:
        case ACPI_DMT_SPACEID:
        case ACPI_DMT_IVRS:
        case ACPI_DMT_MADT:
        case ACPI_DMT_SRAT:
        case ACPI_DMT_ASF:
        case ACPI_DMT_HESTNTYP:
        case ACPI_DMT_FADTPM:
            ByteLength = 1;
            break;
        case ACPI_DMT_UINT16:
        case ACPI_DMT_DMAR:
        case ACPI_DMT_HEST:
            ByteLength = 2;
            break;
        case ACPI_DMT_UINT24:
            ByteLength = 3;
            break;
        case ACPI_DMT_UINT32:
        case ACPI_DMT_NAME4:
        case ACPI_DMT_SIG:
            ByteLength = 4;
            break;
        case ACPI_DMT_NAME6:
            ByteLength = 6;
            break;
        case ACPI_DMT_UINT56:
            ByteLength = 7;
            break;
        case ACPI_DMT_UINT64:
        case ACPI_DMT_NAME8:
            ByteLength = 8;
            break;
        case ACPI_DMT_BUF16:
            ByteLength = 16;
            break;
        case ACPI_DMT_STRING:
            ByteLength = ACPI_STRLEN (ACPI_CAST_PTR (char, Target)) + 1;
            break;
        case ACPI_DMT_GAS:
            if (!LastOutputBlankLine)
            {
                AcpiOsPrintf ("\n");
                LastOutputBlankLine = TRUE;
            }
            ByteLength = sizeof (ACPI_GENERIC_ADDRESS);
            break;
        case ACPI_DMT_HESTNTFY:
            if (!LastOutputBlankLine)
            {
                AcpiOsPrintf ("\n");
                LastOutputBlankLine = TRUE;
            }
            ByteLength = sizeof (ACPI_HEST_NOTIFY);
            break;
        default:
            ByteLength = 0;
            break;
        }

        if (CurrentOffset + ByteLength > TableLength)
        {
            AcpiOsPrintf ("**** ACPI table terminates in the middle of a data structure!\n");
            return (AE_BAD_DATA);
        }

        /* Start a new line and decode the opcode */

        AcpiDmLineHeader (CurrentOffset, ByteLength, Info->Name);

        switch (Info->Opcode)
        {
        /* Single-bit Flag fields. Note: Opcode is the bit position */

        case ACPI_DMT_FLAG0:
        case ACPI_DMT_FLAG1:
        case ACPI_DMT_FLAG2:
        case ACPI_DMT_FLAG3:
        case ACPI_DMT_FLAG4:
        case ACPI_DMT_FLAG5:
        case ACPI_DMT_FLAG6:
        case ACPI_DMT_FLAG7:

            AcpiOsPrintf ("%1.1X\n", (*Target >> Info->Opcode) & 0x01);
            break;

        /* 2-bit Flag fields */

        case ACPI_DMT_FLAGS0:

            AcpiOsPrintf ("%1.1X\n", *Target & 0x03);
            break;

        case ACPI_DMT_FLAGS2:

            AcpiOsPrintf ("%1.1X\n", (*Target >> 2) & 0x03);
            break;

        /* Standard Data Types */

        case ACPI_DMT_UINT8:

            AcpiOsPrintf ("%2.2X\n", *Target);
            break;

        case ACPI_DMT_UINT16:

            AcpiOsPrintf ("%4.4X\n", ACPI_GET16 (Target));
            break;

        case ACPI_DMT_UINT24:

            AcpiOsPrintf ("%2.2X%2.2X%2.2X\n",
                *Target, *(Target + 1), *(Target + 2));
            break;

        case ACPI_DMT_UINT32:

            AcpiOsPrintf ("%8.8X\n", ACPI_GET32 (Target));
            break;

        case ACPI_DMT_UINT56:

            for (Temp8 = 0; Temp8 < 7; Temp8++)
            {
                AcpiOsPrintf ("%2.2X", Target[Temp8]);
            }
            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_UINT64:

            AcpiOsPrintf ("%8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (ACPI_GET64 (Target)));
            break;

        case ACPI_DMT_BUF16:

            /* Buffer of length 16 */

            for (Temp8 = 0; Temp8 < 16; Temp8++)
            {
                AcpiOsPrintf ("%2.2X,", Target[Temp8]);
            }
            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_STRING:

            AcpiOsPrintf ("\"%s\"\n", ACPI_CAST_PTR (char, Target));
            break;

        /* Fixed length ASCII name fields */

        case ACPI_DMT_SIG:

            AcpiDmCheckAscii (Target, RepairedName, 4);
            AcpiOsPrintf ("\"%.4s\"    ", RepairedName);
            TableData = AcpiDmGetTableData (ACPI_CAST_PTR (char, Target));
            if (TableData)
            {
                AcpiOsPrintf ("/* %s */", TableData->Name);
            }
            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_NAME4:

            AcpiDmCheckAscii (Target, RepairedName, 4);
            AcpiOsPrintf ("\"%.4s\"\n", RepairedName);
            break;

        case ACPI_DMT_NAME6:

            AcpiDmCheckAscii (Target, RepairedName, 6);
            AcpiOsPrintf ("\"%.6s\"\n", RepairedName);
            break;

        case ACPI_DMT_NAME8:

            AcpiDmCheckAscii (Target, RepairedName, 8);
            AcpiOsPrintf ("\"%.8s\"\n", RepairedName);
            break;

        /* Special Data Types */

        case ACPI_DMT_CHKSUM:

            /* Checksum, display and validate */

            AcpiOsPrintf ("%2.2X", *Target);
            Temp8 = AcpiTbGenerateChecksum (Table);
            if (Temp8 != ACPI_CAST_PTR (ACPI_TABLE_HEADER, Table)->Checksum)
            {
                AcpiOsPrintf (
                    "     /* Incorrect checksum, should be %2.2X */", Temp8);
            }
            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_SPACEID:

            /* Address Space ID */

            AcpiOsPrintf ("%2.2X (%s)\n", *Target, AcpiUtGetRegionName (*Target));
            break;

        case ACPI_DMT_GAS:

            /* Generic Address Structure */

            AcpiOsPrintf ("<Generic Address Structure>\n");
            AcpiDmDumpTable (TableLength, CurrentOffset, Target,
                sizeof (ACPI_GENERIC_ADDRESS), AcpiDmTableInfoGas);
            AcpiOsPrintf ("\n");
            LastOutputBlankLine = TRUE;
            break;

        case ACPI_DMT_ASF:

            /* ASF subtable types */

            Temp16 = (UINT16) ((*Target) & 0x7F);  /* Top bit can be zero or one */
            if (Temp16 > ACPI_ASF_TYPE_RESERVED)
            {
                Temp16 = ACPI_ASF_TYPE_RESERVED;
            }

            AcpiOsPrintf ("%2.2X <%s>\n", *Target, AcpiDmAsfSubnames[Temp16]);
            break;

        case ACPI_DMT_DMAR:

            /* DMAR subtable types */

            Temp16 = ACPI_GET16 (Target);
            if (Temp16 > ACPI_DMAR_TYPE_RESERVED)
            {
                Temp16 = ACPI_DMAR_TYPE_RESERVED;
            }

            AcpiOsPrintf ("%4.4X <%s>\n", ACPI_GET16 (Target), AcpiDmDmarSubnames[Temp16]);
            break;

        case ACPI_DMT_HEST:

            /* HEST subtable types */

            Temp16 = ACPI_GET16 (Target);
            if (Temp16 > ACPI_HEST_TYPE_RESERVED)
            {
                Temp16 = ACPI_HEST_TYPE_RESERVED;
            }

            AcpiOsPrintf ("%4.4X (%s)\n", ACPI_GET16 (Target), AcpiDmHestSubnames[Temp16]);
            break;

        case ACPI_DMT_HESTNTFY:

            AcpiOsPrintf ("<Hardware Error Notification Structure>\n");
            AcpiDmDumpTable (TableLength, CurrentOffset, Target,
                sizeof (ACPI_HEST_NOTIFY), AcpiDmTableInfoHestNotify);
            AcpiOsPrintf ("\n");
            LastOutputBlankLine = TRUE;
            break;

        case ACPI_DMT_HESTNTYP:

            /* HEST Notify types */

            Temp8 = *Target;
            if (Temp8 > ACPI_HEST_NOTIFY_RESERVED)
            {
                Temp8 = ACPI_HEST_NOTIFY_RESERVED;
            }

            AcpiOsPrintf ("%2.2X (%s)\n", *Target, AcpiDmHestNotifySubnames[Temp8]);
            break;


        case ACPI_DMT_MADT:

            /* MADT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_MADT_TYPE_RESERVED)
            {
                Temp8 = ACPI_MADT_TYPE_RESERVED;
            }

            AcpiOsPrintf ("%2.2X <%s>\n", *Target, AcpiDmMadtSubnames[Temp8]);
            break;

        case ACPI_DMT_SRAT:

            /* SRAT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_SRAT_TYPE_RESERVED)
            {
                Temp8 = ACPI_SRAT_TYPE_RESERVED;
            }

            AcpiOsPrintf ("%2.2X <%s>\n", *Target, AcpiDmSratSubnames[Temp8]);
            break;

        case ACPI_DMT_FADTPM:

            /* FADT Preferred PM Profile names */

            Temp8 = *Target;
            if (Temp8 > ACPI_FADT_PM_RESERVED)
            {
                Temp8 = ACPI_FADT_PM_RESERVED;
            }

            AcpiOsPrintf ("%2.2X (%s)\n", *Target, AcpiDmFadtProfiles[Temp8]);
            break;

        case ACPI_DMT_IVRS:

            /* IVRS subtable types */

            Temp8 = *Target;
            switch (Temp8)
            {
            case ACPI_IVRS_TYPE_HARDWARE:
                Name = AcpiDmIvrsSubnames[0];
                break;

            case ACPI_IVRS_TYPE_MEMORY1:
            case ACPI_IVRS_TYPE_MEMORY2:
            case ACPI_IVRS_TYPE_MEMORY3:
                Name = AcpiDmIvrsSubnames[1];
                break;

            default:
                Name = AcpiDmIvrsSubnames[2];
                break;
            }

            AcpiOsPrintf ("%2.2X <%s>\n", *Target, Name);
            break;

        case ACPI_DMT_EXIT:
            return (AE_OK);

        default:
            ACPI_ERROR ((AE_INFO,
                "**** Invalid table opcode [%X] ****\n", Info->Opcode));
            return (AE_SUPPORT);
        }
    }

    if (TableOffset && !SubtableLength)
    {
        /* If this table is not the main table, subtable must have valid length */

        AcpiOsPrintf ("Invalid zero length subtable\n");
        return (AE_BAD_DATA);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCheckAscii
 *
 * PARAMETERS:  Name                - Ascii string
 *              Count               - Number of characters to check
 *
 * RETURN:      None
 *
 * DESCRIPTION: Ensure that the requested number of characters are printable
 *              Ascii characters. Sets non-printable and null chars to <space>.
 *
 ******************************************************************************/

static void
AcpiDmCheckAscii (
    UINT8                   *Name,
    char                    *RepairedName,
    UINT32                  Count)
{
    UINT32                  i;


    for (i = 0; i < Count; i++)
    {
        RepairedName[i] = (char) Name[i];

        if (!Name[i])
        {
            return;
        }
        if (!isprint (Name[i]))
        {
            RepairedName[i] = ' ';
        }
    }
}
