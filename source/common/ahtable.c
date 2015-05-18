/******************************************************************************
 *
 * Module Name: ahtable - Table of known ACPI tables with descriptions
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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


/* Local prototypes */

const AH_TABLE *
AcpiAhGetTableInfo (
    char                    *Signature);

extern const AH_TABLE      AcpiSupportedTables[];


/*******************************************************************************
 *
 * FUNCTION:    AcpiAhGetTableInfo
 *
 * PARAMETERS:  Signature           - ACPI signature (4 chars) to match
 *
 * RETURN:      Pointer to a valid AH_TABLE. Null if no match found.
 *
 * DESCRIPTION: Find a match in the "help" table of supported ACPI tables
 *
 ******************************************************************************/

const AH_TABLE *
AcpiAhGetTableInfo (
    char                    *Signature)
{
    const AH_TABLE      *Info;


    for (Info = AcpiSupportedTables; Info->Signature; Info++)
    {
        if (ACPI_COMPARE_NAME (Signature, Info->Signature))
        {
            return (Info);
        }
    }

    return (NULL);
}


/*
 * Note: Any tables added here should be duplicated within AcpiDmTableData
 * in the file common/dmtable.c
 */
const AH_TABLE      AcpiSupportedTables[] =
{
    {ACPI_SIG_ASF,  "Alert Standard Format table"},
    {ACPI_SIG_BERT, "Boot Error Record Table"},
    {ACPI_SIG_BGRT, "Boot Graphics Resource Table"},
    {ACPI_SIG_BOOT, "Simple Boot Flag Table"},
    {ACPI_SIG_CPEP, "Corrected Platform Error Polling table"},
    {ACPI_SIG_CSRT, "Core System Resource Table"},
    {ACPI_SIG_DBG2, "Debug Port table type 2"},
    {ACPI_SIG_DBGP, "Debug Port table"},
    {ACPI_SIG_DMAR, "DMA Remapping table"},
    {ACPI_SIG_DRTM, "Dynamic Root of Trust for Measurement table"},
    {ACPI_SIG_DSDT, "Differentiated System Description Table (AML table)"},
    {ACPI_SIG_ECDT, "Embedded Controller Boot Resources Table"},
    {ACPI_SIG_EINJ, "Error Injection table"},
    {ACPI_SIG_ERST, "Error Record Serialization Table"},
    {ACPI_SIG_FACS, "Firmware ACPI Control Structure"},
    {ACPI_SIG_FADT, "Fixed ACPI Description Table (FADT)"},
    {ACPI_SIG_FPDT, "Firmware Performance Data Table"},
    {ACPI_SIG_GTDT, "Generic Timer Description Table"},
    {ACPI_SIG_HEST, "Hardware Error Source Table"},
    {ACPI_SIG_HPET, "High Precision Event Timer table"},
    {ACPI_SIG_IORT, "IO Remapping Table"},
    {ACPI_SIG_IVRS, "I/O Virtualization Reporting Structure"},
    {ACPI_SIG_LPIT, "Low Power Idle Table"},
    {ACPI_SIG_MADT, "Multiple APIC Description Table (MADT)"},
    {ACPI_SIG_MCFG, "Memory Mapped Configuration table"},
    {ACPI_SIG_MCHI, "Management Controller Host Interface table"},
    {ACPI_SIG_MPST, "Memory Power State Table"},
    {ACPI_SIG_MSCT, "Maximum System Characteristics Table"},
    {ACPI_SIG_MSDM, "Microsoft Data Management table"},
    {ACPI_SIG_MTMR, "MID Timer Table"},
    {ACPI_SIG_NFIT, "NVDIMM Firmware Interface Table"},
    {ACPI_SIG_PCCT, "Platform Communications Channel Table"},
    {ACPI_SIG_PMTT, "Platform Memory Topology Table"},
    {ACPI_RSDP_NAME,"Root System Description Pointer"},
    {ACPI_SIG_RSDT, "Root System Description Table"},
    {ACPI_SIG_S3PT, "S3 Performance Table"},
    {ACPI_SIG_SBST, "Smart Battery Specification Table"},
    {ACPI_SIG_SLIC, "Software Licensing Description Table"},
    {ACPI_SIG_SLIT, "System Locality Information Table"},
    {ACPI_SIG_SPCR, "Serial Port Console Redirection table"},
    {ACPI_SIG_SPMI, "Server Platform Management Interface table"},
    {ACPI_SIG_SRAT, "System Resource Affinity Table"},
    {ACPI_SIG_SSDT, "Secondary System Description Table (AML table)"},
    {ACPI_SIG_STAO, "Status Override table"},
    {ACPI_SIG_TCPA, "Trusted Computing Platform Alliance table"},
    {ACPI_SIG_TPM2, "Trusted Platform Module hardware interface table"},
    {ACPI_SIG_UEFI, "UEFI Boot Optimization Table"},
    {ACPI_SIG_VRTC, "Virtual Real-Time Clock Table"},
    {ACPI_SIG_WAET, "Windows ACPI Emulated Devices Table"},
    {ACPI_SIG_WDAT, "Watchdog Action Table"},
    {ACPI_SIG_WDDT, "Watchdog Description Table"},
    {ACPI_SIG_WDRT, "Watchdog Resource Table"},
    {ACPI_SIG_WPBT, "Windows Platform Binary Table"},
    {ACPI_SIG_XENV, "Xen Environment table"},
    {ACPI_SIG_XSDT, "Extended System Description Table"},
    {NULL,          NULL}
};
