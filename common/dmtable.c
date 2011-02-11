/******************************************************************************
 *
 * Module Name: dmtable - Support for ACPI tables that contain no AML code
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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

#include "acpi.h"
#include "accommon.h"
#include "acdisasm.h"
#include "actables.h"
#include "aslcompiler.h"
#include "dtcompiler.h"

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtable")

/* Local Prototypes */

static void
AcpiDmCheckAscii (
    UINT8                   *Target,
    char                    *RepairedName,
    UINT32                  Count);


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

static const char           *AcpiDmEinjActions[] =
{
    "Begin Operation",
    "Get Trigger Table",
    "Set Error Type",
    "Get Error Type",
    "End Operation",
    "Execute Operation",
    "Check Busy Status",
    "Get Command Status",
    "Unknown Action"
};

static const char           *AcpiDmEinjInstructions[] =
{
    "Read Register",
    "Read Register Value",
    "Write Register",
    "Write Register Value",
    "Noop",
    "Unknown Instruction"
};

static const char           *AcpiDmErstActions[] =
{
    "Begin Write Operation",
    "Begin Read Operation",
    "Begin Clear Operation",
    "End Operation",
    "Set Record Offset",
    "Execute Operation",
    "Check Busy Status",
    "Get Command Status",
    "Get Record Identifier",
    "Set Record Identifier",
    "Get Record Count",
    "Begin Dummy Write",
    "Unused/Unknown Action",
    "Get Error Address Range",
    "Get Error Address Length",
    "Get Error Attributes",
    "Unknown Action"
};

static const char           *AcpiDmErstInstructions[] =
{
    "Read Register",
    "Read Register Value",
    "Write Register",
    "Write Register Value",
    "Noop",
    "Load Var1",
    "Load Var2",
    "Store Var1",
    "Add",
    "Subtract",
    "Add Value",
    "Subtract Value",
    "Stall",
    "Stall While True",
    "Skip Next If True",
    "GoTo",
    "Set Source Address",
    "Set Destination Address",
    "Move Data",
    "Unknown Instruction"
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

#define ACPI_GAS_WIDTH_RESERVED     5

static const char           *AcpiDmGasAccessWidth[] =
{
    "Undefined/Legacy",
    "Byte Access:8",
    "Word Access:16",
    "DWord Access:32",
    "QWord Access:64",
    "Unknown Width Encoding"
};


/*******************************************************************************
 *
 * ACPI Table Data, indexed by signature.
 *
 * Each entry contains: Signature, Table Info, Handler, DtHandler,
 *  Template, Description
 *
 * Simple tables have only a TableInfo structure, complex tables have a
 * handler. This table must be NULL terminated. RSDP and FACS are
 * special-cased elsewhere.
 *
 ******************************************************************************/

ACPI_DMTABLE_DATA    AcpiDmTableData[] =
{
    {ACPI_SIG_ASF,  NULL,                   AcpiDmDumpAsf,  DtCompileAsf,   TemplateAsf,    "Alert Standard Format table"},
    {ACPI_SIG_BOOT, AcpiDmTableInfoBoot,    NULL,           NULL,           TemplateBoot,   "Simple Boot Flag Table"},
    {ACPI_SIG_BERT, AcpiDmTableInfoBert,    NULL,           NULL,           TemplateBert,   "Boot Error Record Table"},
    {ACPI_SIG_CPEP, NULL,                   AcpiDmDumpCpep, DtCompileCpep,  TemplateCpep,   "Corrected Platform Error Polling table"},
    {ACPI_SIG_DBGP, AcpiDmTableInfoDbgp,    NULL,           NULL,           TemplateDbgp,   "Debug Port table"},
    {ACPI_SIG_DMAR, NULL,                   AcpiDmDumpDmar, DtCompileDmar,  TemplateDmar,   "DMA Remapping table"},
    {ACPI_SIG_ECDT, AcpiDmTableInfoEcdt,    NULL,           NULL,           TemplateEcdt,   "Embedded Controller Boot Resources Table"},
    {ACPI_SIG_EINJ, NULL,                   AcpiDmDumpEinj, DtCompileEinj,  TemplateEinj,   "Error Injection table"},
    {ACPI_SIG_ERST, NULL,                   AcpiDmDumpErst, DtCompileErst,  TemplateErst,   "Error Record Serialization Table"},
    {ACPI_SIG_FADT, NULL,                   AcpiDmDumpFadt, DtCompileFadt,  TemplateFadt,   "Fixed ACPI Description Table"},
    {ACPI_SIG_HEST, NULL,                   AcpiDmDumpHest, DtCompileHest,  TemplateHest,   "Hardware Error Source Table"},
    {ACPI_SIG_HPET, AcpiDmTableInfoHpet,    NULL,           NULL,           TemplateHpet,   "High Precision Event Timer table"},
    {ACPI_SIG_IVRS, NULL,                   AcpiDmDumpIvrs, DtCompileIvrs,  TemplateIvrs,   "I/O Virtualization Reporting Structure"},
    {ACPI_SIG_MADT, NULL,                   AcpiDmDumpMadt, DtCompileMadt,  TemplateMadt,   "Multiple APIC Description Table"},
    {ACPI_SIG_MCFG, NULL,                   AcpiDmDumpMcfg, DtCompileMcfg,  TemplateMcfg,   "Memory Mapped Configuration table"},
    {ACPI_SIG_MCHI, AcpiDmTableInfoMchi,    NULL,           NULL,           TemplateMchi,   "Management Controller Host Interface table"},
    {ACPI_SIG_MSCT, NULL,                   AcpiDmDumpMsct, DtCompileMsct,  TemplateMsct,   "Maximum System Characteristics Table"},
    {ACPI_SIG_RSDT, NULL,                   AcpiDmDumpRsdt, DtCompileRsdt,  TemplateRsdt,   "Root System Description Table"},
    {ACPI_SIG_SBST, AcpiDmTableInfoSbst,    NULL,           NULL,           TemplateSbst,   "Smart Battery Specification Table"},
    {ACPI_SIG_SLIC, AcpiDmTableInfoSlic,    NULL,           NULL,           NULL,           "Software Licensing Description Table"},
    {ACPI_SIG_SLIT, NULL,                   AcpiDmDumpSlit, DtCompileSlit,  TemplateSlit,   "System Locality Information Table"},
    {ACPI_SIG_SPCR, AcpiDmTableInfoSpcr,    NULL,           NULL,           TemplateSpcr,   "Serial Port Console Redirection table"},
    {ACPI_SIG_SPMI, AcpiDmTableInfoSpmi,    NULL,           NULL,           TemplateSpmi,   "Server Platform Management Interface table"},
    {ACPI_SIG_SRAT, NULL,                   AcpiDmDumpSrat, DtCompileSrat,  TemplateSrat,   "System Resource Affinity Table"},
    {ACPI_SIG_TCPA, AcpiDmTableInfoTcpa,    NULL,           NULL,           TemplateTcpa,   "Trusted Computing Platform Alliance table"},
    {ACPI_SIG_UEFI, AcpiDmTableInfoUefi,    NULL,           DtCompileUefi,  TemplateUefi,   "UEFI Boot Optimization Table"},
    {ACPI_SIG_WAET, AcpiDmTableInfoWaet,    NULL,           NULL,           TemplateWaet,   "Windows ACPI Emulated Devices Table"},
    {ACPI_SIG_WDAT, NULL,                   AcpiDmDumpWdat, DtCompileWdat,  TemplateWdat,   "Watchdog Action Table"},
    {ACPI_SIG_WDDT, AcpiDmTableInfoWddt,    NULL,           NULL,           TemplateWddt,   "Watchdog Description Table"},
    {ACPI_SIG_WDRT, AcpiDmTableInfoWdrt,    NULL,           NULL,           TemplateWdrt,   "Watchdog Resource Table"},
    {ACPI_SIG_XSDT, NULL,                   AcpiDmDumpXsdt, DtCompileXsdt,  TemplateXsdt,   "Extended System Description Table"},
    {NULL,          NULL,                   NULL,           NULL,           NULL,           NULL}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmGenerateChecksum
 *
 * PARAMETERS:  Table               - Pointer to table to be checksummed
 *              Length              - Length of the table
 *              OriginalChecksum    - Value of the checksum field
 *
 * RETURN:      8 bit checksum of buffer
 *
 * DESCRIPTION: Computes an 8 bit checksum of the table.
 *
 ******************************************************************************/

UINT8
AcpiDmGenerateChecksum (
    void                    *Table,
    UINT32                  Length,
    UINT8                   OriginalChecksum)
{
    UINT8                   Checksum;


    /* Sum the entire table as-is */

    Checksum = AcpiTbChecksum ((UINT8 *) Table, Length);

    /* Subtract off the existing checksum value in the table */

    Checksum = (UINT8) (Checksum - OriginalChecksum);

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

ACPI_DMTABLE_DATA *
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

    if (!Gbl_DoTemplates || Gbl_VerboseTemplates)
    {
        /* Dump the raw table data */

        AcpiOsPrintf ("\n%s: Length %d (0x%X)\n\n",
            ACPI_RAW_TABLE_DATA_HEADER, Length, Length);
        AcpiUtDumpBuffer2 (ACPI_CAST_PTR (UINT8, Table), Length, DB_BYTE_DISPLAY);
    }
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

    if (Gbl_DoTemplates && !Gbl_VerboseTemplates) /* Terse template */
    {
        if (ByteLength)
        {
            AcpiOsPrintf ("[%.3d] %34s : ",
                ByteLength, Name);
        }
        else
        {
            AcpiOsPrintf ("%40s : ",
                Name);
        }
    }
    else /* Normal disassembler or verbose template */
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
}

void
AcpiDmLineHeader2 (
    UINT32                  Offset,
    UINT32                  ByteLength,
    char                    *Name,
    UINT32                  Value)
{

    if (Gbl_DoTemplates && !Gbl_VerboseTemplates) /* Terse template */
    {
        if (ByteLength)
        {
            AcpiOsPrintf ("[%.3d] %30s % 3d : ",
                ByteLength, Name, Value);
        }
        else
        {
            AcpiOsPrintf ("%36s % 3d : ",
                Name, Value);
        }
    }
    else /* Normal disassembler or verbose template */
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
 * Note: This function must remain in sync with DtGetFieldLength.
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
        case ACPI_DMT_ACCWIDTH:
        case ACPI_DMT_IVRS:
        case ACPI_DMT_MADT:
        case ACPI_DMT_SRAT:
        case ACPI_DMT_ASF:
        case ACPI_DMT_HESTNTYP:
        case ACPI_DMT_FADTPM:
        case ACPI_DMT_EINJACT:
        case ACPI_DMT_EINJINST:
        case ACPI_DMT_ERSTACT:
        case ACPI_DMT_ERSTINST:
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
        case ACPI_DMT_BUF7:
            ByteLength = 7;
            break;
        case ACPI_DMT_UINT64:
        case ACPI_DMT_NAME8:
            ByteLength = 8;
            break;
        case ACPI_DMT_BUF16:
        case ACPI_DMT_UUID:
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

        case ACPI_DMT_BUF7:
        case ACPI_DMT_BUF16:

            /*
             * Buffer: Size depends on the opcode and was set above.
             * Each hex byte is separated with a space.
             */
            for (Temp8 = 0; Temp8 < ByteLength; Temp8++)
            {
                AcpiOsPrintf ("%2.2X", Target[Temp8]);
                if ((UINT32) (Temp8 + 1) < ByteLength)
                {
                    AcpiOsPrintf (" ");
                }
            }
            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_UUID:

            /* Convert 16-byte UUID buffer to 36-byte formatted UUID string */

            (void) AuConvertUuidToString ((char *) Target, MsgBuffer);

            AcpiOsPrintf ("%s\n", MsgBuffer);
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
            Temp8 = AcpiDmGenerateChecksum (Table,
                        ACPI_CAST_PTR (ACPI_TABLE_HEADER, Table)->Length,
                        ACPI_CAST_PTR (ACPI_TABLE_HEADER, Table)->Checksum);
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

        case ACPI_DMT_ACCWIDTH:

            /* Encoded Access Width */

            Temp8 = *Target;
            if (Temp8 > ACPI_GAS_WIDTH_RESERVED)
            {
                Temp8 = ACPI_GAS_WIDTH_RESERVED;
            }

            AcpiOsPrintf ("%2.2X (%s)\n", Temp8, AcpiDmGasAccessWidth[Temp8]);
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

        case ACPI_DMT_EINJACT:

            /* EINJ Action types */

            Temp8 = *Target;
            if (Temp8 > ACPI_EINJ_ACTION_RESERVED)
            {
                Temp8 = ACPI_EINJ_ACTION_RESERVED;
            }

            AcpiOsPrintf ("%2.2X (%s)\n", *Target, AcpiDmEinjActions[Temp8]);
            break;

        case ACPI_DMT_EINJINST:

            /* EINJ Instruction types */

            Temp8 = *Target;
            if (Temp8 > ACPI_EINJ_INSTRUCTION_RESERVED)
            {
                Temp8 = ACPI_EINJ_INSTRUCTION_RESERVED;
            }

            AcpiOsPrintf ("%2.2X (%s)\n", *Target, AcpiDmEinjInstructions[Temp8]);
            break;

        case ACPI_DMT_ERSTACT:

            /* ERST Action types */

            Temp8 = *Target;
            if (Temp8 > ACPI_ERST_ACTION_RESERVED)
            {
                Temp8 = ACPI_ERST_ACTION_RESERVED;
            }

            AcpiOsPrintf ("%2.2X (%s)\n", *Target, AcpiDmErstActions[Temp8]);
            break;

        case ACPI_DMT_ERSTINST:

            /* ERST Instruction types */

            Temp8 = *Target;
            if (Temp8 > ACPI_ERST_INSTRUCTION_RESERVED)
            {
                Temp8 = ACPI_ERST_INSTRUCTION_RESERVED;
            }

            AcpiOsPrintf ("%2.2X (%s)\n", *Target, AcpiDmErstInstructions[Temp8]);
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
                "**** Invalid table opcode [0x%X] ****\n", Info->Opcode));
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
