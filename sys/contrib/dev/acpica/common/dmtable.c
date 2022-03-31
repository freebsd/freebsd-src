/******************************************************************************
 *
 * Module Name: dmtable - Support for ACPI tables that contain no AML code
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2022, Intel Corp.
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/compiler/aslcompiler.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtable")

const AH_TABLE *
AcpiAhGetTableInfo (
    char                    *Signature);


/* Common format strings for commented values */

#define UINT8_FORMAT            "%2.2X [%s]\n"
#define UINT8_FORMAT_NO_NEWLINE "%2.2X [%s]"
#define UINT16_FORMAT           "%4.4X [%s]\n"
#define UINT32_FORMAT           "%8.8X [%s]\n"
#define STRING_FORMAT           "[%s]\n"

/* These tables map a subtable type to a description string */

static const char           *AcpiDmAestResourceNames[] =
{
    "Cache Resource",
    "TLB Resource",
    "Generic Resource",
    "Unknown Resource Type"         /* Reserved */
};

static const char           *AcpiDmAestSubnames[] =
{
    "Processor Error Node",
    "Memory Error Node",
    "SMMU Error Node",
    "Vendor-defined Error Node",
    "GIC Error Node",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmAestCacheNames[] =
{
    "Data Cache",
    "Instruction Cache",
    "Unified Cache",
    "Unknown Cache Type"            /* Reserved */
};

static const char           *AcpiDmAestGicNames[] =
{
    "GIC CPU",
    "GIC Distributor",
    "GIC Redistributor",
    "GIC ITS",
    "Unknown GIC Interface Type"    /* Reserved */
};

static const char           *AcpiDmAestXfaceNames[] =
{
    "System Register Interface",
    "Memory Mapped Interface",
    "Unknown Interface Type"        /* Reserved */
};

static const char           *AcpiDmAestXruptNames[] =
{
    "Fault Handling Interrupt",
    "Error Recovery Interrupt",
    "Unknown Interrupt Type"        /* Reserved */
};

static const char           *AcpiDmAsfSubnames[] =
{
    "ASF Information",
    "ASF Alerts",
    "ASF Remote Control",
    "ASF RMCP Boot Options",
    "ASF Address",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmCedtSubnames[] =
{
    "CXL Host Bridge Structure",
    "CXL Fixed Memory Window Structure",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmDmarSubnames[] =
{
    "Hardware Unit Definition",
    "Reserved Memory Region",
    "Root Port ATS Capability",
    "Remapping Hardware Static Affinity",
    "ACPI Namespace Device Declaration",
    "SoC Integrated Address Translation Cache",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmDmarScope[] =
{
    "Reserved value",
    "PCI Endpoint Device",
    "PCI Bridge Device",
    "IOAPIC Device",
    "Message-capable HPET Device",
    "Namespace Device",
    "Unknown Scope Type"            /* Reserved */
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
    "Set Error Type With Address",
    "Get Execute Timings",
    "Unknown Action"
};

static const char           *AcpiDmEinjInstructions[] =
{
    "Read Register",
    "Read Register Value",
    "Write Register",
    "Write Register Value",
    "Noop",
    "Flush Cacheline",
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
    "Execute Timings",
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

static const char           *AcpiDmGtdtSubnames[] =
{
    "Generic Timer Block",
    "Generic Watchdog Timer",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmHestSubnames[] =
{
    "IA-32 Machine Check Exception",
    "IA-32 Corrected Machine Check",
    "IA-32 Non-Maskable Interrupt",
    "Unknown Subtable Type",        /* 3 - Reserved */
    "Unknown Subtable Type",        /* 4 - Reserved */
    "Unknown Subtable Type",        /* 5 - Reserved */
    "PCI Express Root Port AER",
    "PCI Express AER (AER Endpoint)",
    "PCI Express/PCI-X Bridge AER",
    "Generic Hardware Error Source",
    "Generic Hardware Error Source V2",
    "IA-32 Deferred Machine Check",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmHestNotifySubnames[] =
{
    "Polled",
    "External Interrupt",
    "Local Interrupt",
    "SCI",
    "NMI",
    "CMCI",                         /* ACPI 5.0 */
    "MCE",                          /* ACPI 5.0 */
    "GPIO",                         /* ACPI 6.0 */
    "SEA",                          /* ACPI 6.1 */
    "SEI",                          /* ACPI 6.1 */
    "GSIV",                         /* ACPI 6.1 */
    "Software Delegated Exception", /* ACPI 6.2 */
    "Unknown Notify Type"           /* Reserved */
};

static const char           *AcpiDmHmatSubnames[] =
{
    "Memory Proximity Domain Attributes",
    "System Locality Latency and Bandwidth Information",
    "Memory Side Cache Information",
    "Unknown Structure Type"         /* Reserved */
};

static const char           *AcpiDmMadtSubnames[] =
{
    "Processor Local APIC",             /* ACPI_MADT_TYPE_LOCAL_APIC */
    "I/O APIC",                         /* ACPI_MADT_TYPE_IO_APIC */
    "Interrupt Source Override",        /* ACPI_MADT_TYPE_INTERRUPT_OVERRIDE */
    "NMI Source",                       /* ACPI_MADT_TYPE_NMI_SOURCE */
    "Local APIC NMI",                   /* ACPI_MADT_TYPE_LOCAL_APIC_NMI */
    "Local APIC Address Override",      /* ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE */
    "I/O SAPIC",                        /* ACPI_MADT_TYPE_IO_SAPIC */
    "Local SAPIC",                      /* ACPI_MADT_TYPE_LOCAL_SAPIC */
    "Platform Interrupt Sources",       /* ACPI_MADT_TYPE_INTERRUPT_SOURCE */
    "Processor Local x2APIC",           /* ACPI_MADT_TYPE_LOCAL_X2APIC */
    "Local x2APIC NMI",                 /* ACPI_MADT_TYPE_LOCAL_X2APIC_NMI */
    "Generic Interrupt Controller",     /* ACPI_MADT_GENERIC_INTERRUPT */
    "Generic Interrupt Distributor",    /* ACPI_MADT_GENERIC_DISTRIBUTOR */
    "Generic MSI Frame",                /* ACPI_MADT_GENERIC_MSI_FRAME */
    "Generic Interrupt Redistributor",  /* ACPI_MADT_GENERIC_REDISTRIBUTOR */
    "Generic Interrupt Translator",     /* ACPI_MADT_GENERIC_TRANSLATOR */
    "Mutiprocessor Wakeup",             /* ACPI_MADT_TYPE_MULTIPROC_WAKEUP */
    "Unknown Subtable Type",            /* Reserved */
    "Types 80-FF are used for OEM data" /* Reserved for OEM data */
};

static const char           *AcpiDmNfitSubnames[] =
{
    "System Physical Address Range",    /* ACPI_NFIT_TYPE_SYSTEM_ADDRESS */
    "Memory Range Map",                 /* ACPI_NFIT_TYPE_MEMORY_MAP */
    "Interleave Info",                  /* ACPI_NFIT_TYPE_INTERLEAVE */
    "SMBIOS Information",               /* ACPI_NFIT_TYPE_SMBIOS */
    "NVDIMM Control Region",            /* ACPI_NFIT_TYPE_CONTROL_REGION */
    "NVDIMM Block Data Window Region",  /* ACPI_NFIT_TYPE_DATA_REGION */
    "Flush Hint Address",               /* ACPI_NFIT_TYPE_FLUSH_ADDRESS */
    "Platform Capabilities",            /* ACPI_NFIT_TYPE_CAPABILITIES */
    "Unknown Subtable Type"             /* Reserved */
};

static const char           *AcpiDmNhltLinkTypeNames[] =
{
    "Reserved for HD-Audio",            /* ACPI_NHLT_RESERVED_HD_AUDIO */
    "Reserved for DSP",                 /* ACPI_NHLT_RESERVED_DSP */
    "Type PDM",                         /* ACPI_NHLT_PDM */
    "Type SSP",                         /* ACPI_NHLT_SSP */
    "Reserved for SlimBus",             /* ACPI_NHLT_RESERVED_SLIMBUS */
    "Reserved for SoundWire",           /* ACPI_NHLT_RESERVED_SOUNDWIRE */
    "Unknown Link Type"                 /* Reserved */
};

static const char           *AcpiDmNhltDirectionNames[] =
{
    "Render",                           /* ACPI_NHLT_DIR_RENDER */
    "Capture",                          /* ACPI_NHLT_DIR_CAPTURE */
    "Render with Loopback",             /* ACPI_NHLT_DIR_RENDER_LOOPBACK */
    "Feedback for Render",              /* ACPI_NHLT_DIR_RENDER_FEEDBACK */
    "Unknown Direction"                 /* Reserved */
};

static const char           *AcpiDmNhltMicTypeNames[] =
{
    "Omnidirectional",                  /* ACPI_NHLT_MIC_OMNIDIRECTIONAL */
    "Subcardioid",                      /* ACPI_NHLT_MIC_SUBCARDIOID */
    "Cardioid",                         /* ACPI_NHLT_MIC_CARDIOID */
    "SuperCardioid",                    /* ACPI_NHLT_MIC_SUPER_CARDIOID */
    "HyperCardioid",                    /* ACPI_NHLT_MIC_HYPER_CARDIOID */
    "8 Shaped",                         /* ACPI_NHLT_MIC_8_SHAPED */
    "Reserved Mic Type",                /* Reserved */
    "Vendor Defined",                   /* ACPI_NHLT_MIC_VENDOR_DEFINED */
    "Unknown Mic Type"                  /* ACPI_NHLT_MIC_RESERVED */
};

static const char           *AcpiDmNhltMicPositionNames[] =
{
    "Top",                              /* ACPI_NHLT_MIC_POSITION_TOP */
    "Bottom",                           /* ACPI_NHLT_MIC_POSITION_BOTTOM */
    "Left",                             /* ACPI_NHLT_MIC_POSITION_LEFT */
    "Right",                            /* ACPI_NHLT_MIC_POSITION_RIGHT */
    "Front",                            /* ACPI_NHLT_MIC_POSITION_FRONT */
    "Back",                             /* ACPI_NHLT_MIC_POSITION_BACK */
    "Unknown Mic Position"              /* 6 and above are reserved */
};

static const char           *AcpiDmNhltMicArrayTypeNames[] =
{
    "Unknown Array Type",               /* ACPI_NHLT_ARRAY_TYPE_RESERVED */
    "Small Linear 2-element",           /* ACPI_NHLT_SMALL_LINEAR_2ELEMENT */
    "Big Linear 2-element",             /* ACPI_NHLT_BIG_LINEAR_2ELEMENT */
    "Linear 4-element 1st Geometry",    /* ACPI_NHLT_FIRST_GEOMETRY_LINEAR_4ELEMENT */
    "Planar L-shaped 4-element",        /* ACPI_NHLT_PLANAR_LSHAPED_4ELEMENT */
    "Linear 4-element 2nd Geometry",    /* ACPI_NHLT_SECOND_GEOMETRY_LINEAR_4ELEMENT */
    "Vendor Defined"                    /* ACPI_NHLT_VENDOR_DEFINED */
};

static const char           *AcpiDmNhltConfigTypeNames[] =
{
    "Generic Type",                     /* ACPI_NHLT_CONFIG_TYPE_GENERIC */
    "Microphone Array",                 /* ACPI_NHLT_CONFIG_TYPE_MIC_ARRAY */
    "Reserved",                         /* ACPI_NHLT_CONFIG_TYPE_RESERVED */
    "Render Feedback",                  /* ACPI_NHLT_CONFIG_TYPE_RENDER_FEEDBACK */
    "Unknown Config Type"               /* ACPI_NHLT_CONFIG_TYPE_RESERVED */
};

static const char           *AcpiDmPcctSubnames[] =
{
    "Generic Communications Subspace",  /* ACPI_PCCT_TYPE_GENERIC_SUBSPACE */
    "HW-Reduced Comm Subspace",         /* ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE */
    "HW-Reduced Comm Subspace Type2",   /* ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2 */
    "Extended PCC Master Subspace",     /* ACPI_PCCT_TYPE_EXT_PCC_MASTER_SUBSPACE */
    "Extended PCC Slave Subspace",      /* ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE */
    "HW Registers based Comm Subspace", /* ACPI_PCCT_TYPE_HW_REG_COMM_SUBSPACE */
    "Unknown Subtable Type"             /* Reserved */
};

static const char           *AcpiDmPhatSubnames[] =
{
    "Firmware Version Data",        /* ACPI_PHAT_TYPE_FW_VERSION_DATA */
    "Firmware Health Data",         /* ACPI_PHAT_TYPE_FW_HEALTH_DATA */
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmPmttSubnames[] =
{
    "Socket",                       /* ACPI_PMTT_TYPE_SOCKET */
    "Memory Controller",            /* ACPI_PMTT_TYPE_CONTROLLER */
    "Physical Component (DIMM)",    /* ACPI_PMTT_TYPE_DIMM */
    "Unknown Subtable Type",        /* Reserved */
    "Vendor Specific"               /* ACPI_PMTT_TYPE_VENDOR */
};

static const char           *AcpiDmPpttSubnames[] =
{
    "Processor Hierarchy Node",     /* ACPI_PPTT_TYPE_PROCESSOR */
    "Cache Type",                   /* ACPI_PPTT_TYPE_CACHE */
    "ID",                           /* ACPI_PPTT_TYPE_ID */
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmRgrtSubnames[] =
{
    "Unknown/Reserved Image Type",  /* ACPI_RGRT_TYPE_RESERVED0 */
    "Type PNG"                      /* ACPI_RGRT_IMAGE_TYPE_PNG */
};

static const char           *AcpiDmSdevSubnames[] =
{
    "Namespace Device",             /* ACPI_SDEV_TYPE_NAMESPACE_DEVICE */
    "PCIe Endpoint Device",         /* ACPI_SDEV_TYPE_PCIE_ENDPOINT_DEVICE */
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmSratSubnames[] =
{
    "Processor Local APIC/SAPIC Affinity",
    "Memory Affinity",
    "Processor Local x2APIC Affinity",
    "GICC Affinity",
    "GIC ITS Affinity",             /* Acpi 6.2 */
    "Generic Initiator Affinity",   /* Acpi 6.3 */
    "Generic Port Affinity",        /* Acpi 6.4 */
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmTpm2Subnames[] =
{
    "Illegal Start Method value",
    "Reserved",
    "ACPI Start Method",
    "Reserved",
    "Reserved",
    "Reserved",
    "Memory Mapped I/O",
    "Command Response Buffer",
    "Command Response Buffer with ACPI Start Method",
    "Reserved",
    "Reserved",
    "Command Response Buffer with ARM SMC",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmIvrsSubnames[] =
{
    "Hardware Definition Block (IVHD)",
    "Hardware Definition Block - Mixed Format (IVHD)",
    "Memory Definition Block (IVMD)",
    "Unknown/Reserved Subtable Type"            /* Reserved */
};

static const char           *AcpiDmIvrsDevEntryNames[] =
{
    "Unknown/Reserved Device Entry Type",       /* 0- Reserved */
    "Device Entry: Select All Devices",         /* 1 */
    "Device Entry: Select One Device",          /* 2 */
    "Device Entry: Start of Range",             /* 3 */
    "Device Entry: End of Range",               /* 4 */
    "Device Entry: Alias Select",               /* 66 */
    "Device Entry: Alias Start of Range",       /* 67 */
    "Unknown/Reserved Device Entry Type",       /* 68- Reserved */
    "Unknown/Reserved Device Entry Type",       /* 69- Reserved */
    "Device Entry: Extended Select",            /* 70 */
    "Device Entry: Extended Start of Range",    /* 71 */
    "Device Entry: Special Device",             /* 72 */
    "Device Entry: ACPI HID Named Device",      /* 240 */
    "Unknown/Reserved Device Entry Type"        /* Reserved */
};

static const char           *AcpiDmLpitSubnames[] =
{
    "Native C-state Idle Structure",
    "Unknown Subtable Type"         /* Reserved */
};

static const char           *AcpiDmViotSubnames[] =
{
    "Unknown Subtable Type",        /* 0 -Reserved */
    "PCI Range",
    "MMIO Endpoint",
    "VirtIO-PCI IOMMU",
    "VirtIO-MMIO IOMMU",
    "Unknown Subtable Type"         /* Reserved */
};

#define ACPI_FADT_PM_RESERVED       9

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
    "Tablet",
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
 * Note: Any tables added here should be duplicated within
 * AcpiGbl_SupportedTables in the file common/ahtable.c
 *
 ******************************************************************************/

const ACPI_DMTABLE_DATA     AcpiDmTableData[] =
{
    {ACPI_SIG_AEST, NULL,                   AcpiDmDumpAest, DtCompileAest,  TemplateAest},
    {ACPI_SIG_AGDI, AcpiDmTableInfoAgdi,    NULL,           NULL,           TemplateAgdi},
    {ACPI_SIG_APMT, NULL,                   AcpiDmDumpApmt, DtCompileApmt,  TemplateApmt},
    {ACPI_SIG_ASF,  NULL,                   AcpiDmDumpAsf,  DtCompileAsf,   TemplateAsf},
    {ACPI_SIG_BDAT, AcpiDmTableInfoBdat,    NULL,           NULL,           TemplateBdat},
    {ACPI_SIG_BERT, AcpiDmTableInfoBert,    NULL,           NULL,           TemplateBert},
    {ACPI_SIG_BGRT, AcpiDmTableInfoBgrt,    NULL,           NULL,           TemplateBgrt},
    {ACPI_SIG_BOOT, AcpiDmTableInfoBoot,    NULL,           NULL,           TemplateBoot},
    {ACPI_SIG_CEDT, NULL,                   AcpiDmDumpCedt, DtCompileCedt,  TemplateCedt},
    {ACPI_SIG_CPEP, NULL,                   AcpiDmDumpCpep, DtCompileCpep,  TemplateCpep},
    {ACPI_SIG_CSRT, NULL,                   AcpiDmDumpCsrt, DtCompileCsrt,  TemplateCsrt},
    {ACPI_SIG_DBG2, AcpiDmTableInfoDbg2,    AcpiDmDumpDbg2, DtCompileDbg2,  TemplateDbg2},
    {ACPI_SIG_DBGP, AcpiDmTableInfoDbgp,    NULL,           NULL,           TemplateDbgp},
    {ACPI_SIG_DMAR, NULL,                   AcpiDmDumpDmar, DtCompileDmar,  TemplateDmar},
    {ACPI_SIG_DRTM, NULL,                   AcpiDmDumpDrtm, DtCompileDrtm,  TemplateDrtm},
    {ACPI_SIG_ECDT, AcpiDmTableInfoEcdt,    NULL,           NULL,           TemplateEcdt},
    {ACPI_SIG_EINJ, NULL,                   AcpiDmDumpEinj, DtCompileEinj,  TemplateEinj},
    {ACPI_SIG_ERST, NULL,                   AcpiDmDumpErst, DtCompileErst,  TemplateErst},
    {ACPI_SIG_FADT, NULL,                   AcpiDmDumpFadt, DtCompileFadt,  TemplateFadt},
    {ACPI_SIG_FPDT, NULL,                   AcpiDmDumpFpdt, DtCompileFpdt,  TemplateFpdt},
    {ACPI_SIG_GTDT, NULL,                   AcpiDmDumpGtdt, DtCompileGtdt,  TemplateGtdt},
    {ACPI_SIG_HEST, NULL,                   AcpiDmDumpHest, DtCompileHest,  TemplateHest},
    {ACPI_SIG_HMAT, NULL,                   AcpiDmDumpHmat, DtCompileHmat,  TemplateHmat},
    {ACPI_SIG_HPET, AcpiDmTableInfoHpet,    NULL,           NULL,           TemplateHpet},
    {ACPI_SIG_IORT, NULL,                   AcpiDmDumpIort, DtCompileIort,  TemplateIort},
    {ACPI_SIG_IVRS, NULL,                   AcpiDmDumpIvrs, DtCompileIvrs,  TemplateIvrs},
    {ACPI_SIG_LPIT, NULL,                   AcpiDmDumpLpit, DtCompileLpit,  TemplateLpit},
    {ACPI_SIG_MADT, NULL,                   AcpiDmDumpMadt, DtCompileMadt,  TemplateMadt},
    {ACPI_SIG_MCFG, NULL,                   AcpiDmDumpMcfg, DtCompileMcfg,  TemplateMcfg},
    {ACPI_SIG_MCHI, AcpiDmTableInfoMchi,    NULL,           NULL,           TemplateMchi},
    {ACPI_SIG_MPST, AcpiDmTableInfoMpst,    AcpiDmDumpMpst, DtCompileMpst,  TemplateMpst},
    {ACPI_SIG_MSCT, NULL,                   AcpiDmDumpMsct, DtCompileMsct,  TemplateMsct},
    {ACPI_SIG_MSDM, NULL,                   AcpiDmDumpSlic, DtCompileSlic,  TemplateMsdm},
    {ACPI_SIG_NFIT, AcpiDmTableInfoNfit,    AcpiDmDumpNfit, DtCompileNfit,  TemplateNfit},
    {ACPI_SIG_NHLT, AcpiDmTableInfoNhlt,    AcpiDmDumpNhlt, DtCompileNhlt,  TemplateNhlt},
    {ACPI_SIG_PCCT, AcpiDmTableInfoPcct,    AcpiDmDumpPcct, DtCompilePcct,  TemplatePcct},
    {ACPI_SIG_PDTT, AcpiDmTableInfoPdtt,    AcpiDmDumpPdtt, DtCompilePdtt,  TemplatePdtt},
    {ACPI_SIG_PHAT, NULL,                   AcpiDmDumpPhat, DtCompilePhat,  TemplatePhat},
    {ACPI_SIG_PMTT, NULL,                   AcpiDmDumpPmtt, DtCompilePmtt,  TemplatePmtt},
    {ACPI_SIG_PPTT, NULL,                   AcpiDmDumpPptt, DtCompilePptt,  TemplatePptt},
    {ACPI_SIG_PRMT, NULL,                   AcpiDmDumpPrmt, DtCompilePrmt,  TemplatePrmt},
    {ACPI_SIG_RASF, AcpiDmTableInfoRasf,    NULL,           NULL,           TemplateRasf},
    {ACPI_SIG_RGRT, NULL,                   AcpiDmDumpRgrt, DtCompileRgrt,  TemplateRgrt},
    {ACPI_SIG_RSDT, NULL,                   AcpiDmDumpRsdt, DtCompileRsdt,  TemplateRsdt},
    {ACPI_SIG_S3PT, NULL,                   NULL,           NULL,           TemplateS3pt},
    {ACPI_SIG_SBST, AcpiDmTableInfoSbst,    NULL,           NULL,           TemplateSbst},
    {ACPI_SIG_SDEI, AcpiDmTableInfoSdei,    NULL,           NULL,           TemplateSdei},
    {ACPI_SIG_SDEV, AcpiDmTableInfoSdev,    AcpiDmDumpSdev, DtCompileSdev,  TemplateSdev},
    {ACPI_SIG_SLIC, NULL,                   AcpiDmDumpSlic, DtCompileSlic,  TemplateSlic},
    {ACPI_SIG_SLIT, NULL,                   AcpiDmDumpSlit, DtCompileSlit,  TemplateSlit},
    {ACPI_SIG_SPCR, AcpiDmTableInfoSpcr,    NULL,           NULL,           TemplateSpcr},
    {ACPI_SIG_SPMI, AcpiDmTableInfoSpmi,    NULL,           NULL,           TemplateSpmi},
    {ACPI_SIG_SRAT, NULL,                   AcpiDmDumpSrat, DtCompileSrat,  TemplateSrat},
    {ACPI_SIG_STAO, NULL,                   AcpiDmDumpStao, DtCompileStao,  TemplateStao},
    {ACPI_SIG_SVKL, AcpiDmTableInfoSvkl,    AcpiDmDumpSvkl, DtCompileSvkl,  TemplateSvkl},
    {ACPI_SIG_TCPA, NULL,                   AcpiDmDumpTcpa, DtCompileTcpa,  TemplateTcpa},
    {ACPI_SIG_TDEL, AcpiDmTableInfoTdel,    NULL,           NULL,           TemplateTdel},
    {ACPI_SIG_TPM2, AcpiDmTableInfoTpm2,    AcpiDmDumpTpm2, DtCompileTpm2,  TemplateTpm2},
    {ACPI_SIG_UEFI, AcpiDmTableInfoUefi,    NULL,           DtCompileUefi,  TemplateUefi},
    {ACPI_SIG_VIOT, AcpiDmTableInfoViot,    AcpiDmDumpViot, DtCompileViot,  TemplateViot},
    {ACPI_SIG_WAET, AcpiDmTableInfoWaet,    NULL,           NULL,           TemplateWaet},
    {ACPI_SIG_WDAT, NULL,                   AcpiDmDumpWdat, DtCompileWdat,  TemplateWdat},
    {ACPI_SIG_WDDT, AcpiDmTableInfoWddt,    NULL,           NULL,           TemplateWddt},
    {ACPI_SIG_WDRT, AcpiDmTableInfoWdrt,    NULL,           NULL,           TemplateWdrt},
    {ACPI_SIG_WPBT, NULL,                   AcpiDmDumpWpbt, DtCompileWpbt,  TemplateWpbt},
    {ACPI_SIG_WSMT, AcpiDmTableInfoWsmt,    NULL,           NULL,           TemplateWsmt},
    {ACPI_SIG_XENV, AcpiDmTableInfoXenv,    NULL,           NULL,           TemplateXenv},
    {ACPI_SIG_XSDT, NULL,                   AcpiDmDumpXsdt, DtCompileXsdt,  TemplateXsdt},
    {NULL,          NULL,                   NULL,           NULL,           NULL}
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

const ACPI_DMTABLE_DATA *
AcpiDmGetTableData (
    char                    *Signature)
{
    const ACPI_DMTABLE_DATA *Info;


    for (Info = AcpiDmTableData; Info->Signature; Info++)
    {
        if (ACPI_COMPARE_NAMESEG (Signature, Info->Signature))
        {
            return (Info);
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
    const ACPI_DMTABLE_DATA *TableData;
    UINT32                  Length;


    /* Ignore tables that contain AML */

    if (AcpiUtIsAmlTable (Table))
    {
        if (AslGbl_VerboseTemplates)
        {
            /* Dump the raw table data */

            Length = Table->Length;

            AcpiOsPrintf ("\n/*\n%s: Length %d (0x%X)\n\n",
                ACPI_RAW_TABLE_DATA_HEADER, Length, Length);
            AcpiUtDumpBuffer (ACPI_CAST_PTR (UINT8, Table),
                Length, DB_BYTE_DISPLAY, 0);
            AcpiOsPrintf (" */\n");
        }
        return;
    }

    /*
     * Handle tables that don't use the common ACPI table header structure.
     * Currently, these are the FACS, RSDP, and S3PT.
     */
    if (ACPI_COMPARE_NAMESEG (Table->Signature, ACPI_SIG_FACS))
    {
        Length = Table->Length;
        Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoFacs);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
    }
    else if (ACPI_VALIDATE_RSDP_SIG (ACPI_CAST_PTR (ACPI_TABLE_RSDP,
        Table)->Signature))
    {
        Length = AcpiDmDumpRsdp (Table);
    }
    else if (ACPI_COMPARE_NAMESEG (Table->Signature, ACPI_SIG_S3PT))
    {
        Length = AcpiDmDumpS3pt (Table);
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
            if (!strncmp (Table->Signature, "OEM", 3))
            {
                AcpiOsPrintf ("\n**** OEM-defined ACPI table [%4.4s], unknown contents\n\n",
                    Table->Signature);
            }
            else
            {
                AcpiOsPrintf ("\n**** Unknown ACPI table signature [%4.4s]\n\n",
                    Table->Signature);

                fprintf (stderr, "Unknown ACPI table signature [%4.4s], ",
                    Table->Signature);

                if (!AcpiGbl_ForceAmlDisassembly)
                {
                    fprintf (stderr, "decoding ACPI table header only\n");
                }
                else
                {
                    fprintf (stderr, "assuming table contains valid AML code\n");
                }
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

            Status = AcpiDmDumpTable (Length, 0, Table, 0, TableData->TableInfo);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }
    }

    if (!AslGbl_DoTemplates || AslGbl_VerboseTemplates)
    {
        /* Dump the raw table data */

        AcpiOsPrintf ("\n%s: Length %d (0x%X)\n\n",
            ACPI_RAW_TABLE_DATA_HEADER, Length, Length);
        AcpiUtDumpBuffer (ACPI_CAST_PTR (UINT8, Table),
            Length, DB_BYTE_DISPLAY, 0);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmLineHeader
 *
 * PARAMETERS:  Offset              - Current byte offset, from table start
 *              ByteLength          - Length of the field in bytes, 0 for flags
 *              Name                - Name of this field
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

    /* Allow a null name for fields that span multiple lines (large buffers) */

    if (!Name)
    {
        Name = "";
    }

    if (AslGbl_DoTemplates && !AslGbl_VerboseTemplates) /* Terse template */
    {
        if (ByteLength)
        {
            AcpiOsPrintf ("[%.4d] %34s : ", ByteLength, Name);
        }
        else
        {
            if (*Name)
            {
                AcpiOsPrintf ("%41s : ", Name);
            }
            else
            {
                AcpiOsPrintf ("%41s   ", Name);
            }
        }
    }
    else /* Normal disassembler or verbose template */
    {
        if (ByteLength)
        {
            AcpiOsPrintf ("[%3.3Xh %4.4d% 4d] %28s : ",
                Offset, Offset, ByteLength, Name);
        }
        else
        {
            if (*Name)
            {
                AcpiOsPrintf ("%44s : ", Name);
            }
            else
            {
                AcpiOsPrintf ("%44s   ", Name);
            }
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

    if (AslGbl_DoTemplates && !AslGbl_VerboseTemplates) /* Terse template */
    {
        if (ByteLength)
        {
            AcpiOsPrintf ("[%.4d] %30s %3d : ",
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
            AcpiOsPrintf ("[%3.3Xh %4.4d %3d] %24s %3d : ",
                Offset, Offset, ByteLength, Name, Value);
        }
        else
        {
            AcpiOsPrintf ("[%3.3Xh %4.4d   ] %24s %3d : ",
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
 * RETURN:      Status
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
    UINT32                  Temp32;
    UINT64                  Value;
    const AH_TABLE          *TableData;
    const char              *Name;
    BOOLEAN                 LastOutputBlankLine = FALSE;
    ACPI_STATUS             Status;
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

        /* Check for beyond subtable end or (worse) beyond EOT */

        if (SubtableLength && (Info->Offset > SubtableLength))
        {
            AcpiOsPrintf (
                "/**** ACPI subtable terminates early (Len %u) - "
                "may be older version (dump table) */\n", SubtableLength);

            /* Move on to next subtable */

            return (AE_OK);
        }

        if (CurrentOffset >= TableLength)
        {
            AcpiOsPrintf (
                "/**** ACPI table terminates "
                "in the middle of a data structure! (dump table) */\n");
            return (AE_BAD_DATA);
        }

        /* Generate the byte length for this field */

        switch (Info->Opcode)
        {
        case ACPI_DMT_UINT8:
        case ACPI_DMT_CHKSUM:
        case ACPI_DMT_SPACEID:
        case ACPI_DMT_ACCWIDTH:
        case ACPI_DMT_CEDT:
        case ACPI_DMT_IVRS:
        case ACPI_DMT_IVRS_DE:
        case ACPI_DMT_GTDT:
        case ACPI_DMT_MADT:
        case ACPI_DMT_NHLT1:
        case ACPI_DMT_NHLT1a:
        case ACPI_DMT_NHLT1b:
        case ACPI_DMT_NHLT1c:
        case ACPI_DMT_NHLT1d:
        case ACPI_DMT_NHLT1f:
        case ACPI_DMT_PCCT:
        case ACPI_DMT_PMTT:
        case ACPI_DMT_PPTT:
        case ACPI_DMT_RGRT:
        case ACPI_DMT_SDEV:
        case ACPI_DMT_SRAT:
        case ACPI_DMT_AEST:
        case ACPI_DMT_AEST_RES:
        case ACPI_DMT_AEST_XFACE:
        case ACPI_DMT_AEST_XRUPT:
        case ACPI_DMT_ASF:
        case ACPI_DMT_HESTNTYP:
        case ACPI_DMT_FADTPM:
        case ACPI_DMT_EINJACT:
        case ACPI_DMT_EINJINST:
        case ACPI_DMT_ERSTACT:
        case ACPI_DMT_ERSTINST:
        case ACPI_DMT_DMAR_SCOPE:
        case ACPI_DMT_VIOT:

            ByteLength = 1;
            break;

        case ACPI_DMT_UINT16:
        case ACPI_DMT_DMAR:
        case ACPI_DMT_HEST:
        case ACPI_DMT_HMAT:
        case ACPI_DMT_NFIT:
        case ACPI_DMT_NHLT1e:
        case ACPI_DMT_PHAT:

            ByteLength = 2;
            break;

        case ACPI_DMT_UINT24:

            ByteLength = 3;
            break;

        case ACPI_DMT_UINT32:
        case ACPI_DMT_AEST_CACHE:
        case ACPI_DMT_AEST_GIC:
        case ACPI_DMT_NAME4:
        case ACPI_DMT_SIG:
        case ACPI_DMT_LPIT:
        case ACPI_DMT_TPM2:

            ByteLength = 4;
            break;

        case ACPI_DMT_UINT40:

            ByteLength = 5;
            break;

        case ACPI_DMT_UINT48:
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

        case ACPI_DMT_BUF10:

            ByteLength = 10;
            break;

        case ACPI_DMT_BUF12:

            ByteLength = 12;
            break;

        case ACPI_DMT_BUF16:
        case ACPI_DMT_UUID:

            ByteLength = 16;
            break;

        case ACPI_DMT_BUF18:

            ByteLength = 18;
            break;

        case ACPI_DMT_BUF128:

            ByteLength = 128;
            break;

        case ACPI_DMT_WPBT_UNICODE:

            ByteLength = SubtableLength;
            CurrentOffset = sizeof (ACPI_TABLE_WPBT);
            break;

        case ACPI_DMT_UNICODE:
        case ACPI_DMT_BUFFER:
        case ACPI_DMT_RAW_BUFFER:

            ByteLength = SubtableLength;
            break;

        case ACPI_DMT_PMTT_VENDOR:
            /*
             * Calculate the length of the vendor data for the PMTT table:
             * Length = (Current Subtable ptr + Subtable length) -
             *          Start of the vendor data (Target)
             */
            ByteLength = ((ACPI_CAST_PTR (char, Table) +
                            (ACPI_CAST_PTR (ACPI_PMTT_HEADER, Table)->Length)) -
                            ACPI_CAST_PTR (char, Target));
            break;

        case ACPI_DMT_STRING:

            ByteLength = strlen (ACPI_CAST_PTR (char, Target)) + 1;
            break;

        case ACPI_DMT_IVRS_UNTERMINATED_STRING:

            ByteLength = ((ACPI_CAST_PTR (ACPI_IVRS_DEVICE_HID, Target) -1)->UidLength);
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

        case ACPI_DMT_IORTMEM:

            if (!LastOutputBlankLine)
            {
                LastOutputBlankLine = FALSE;
            }

            ByteLength = sizeof (ACPI_IORT_MEMORY_ACCESS);
            break;

        default:

            ByteLength = 0;
            break;
        }

        /* Check if we are beyond a subtable, or (worse) beyond EOT */

        if (CurrentOffset + ByteLength > TableLength)
        {
            if (SubtableLength)
            {
                AcpiOsPrintf (
                    "/**** ACPI subtable terminates early - "
                    "may be older version (dump table) */\n");

                /* Move on to next subtable */

                return (AE_OK);
            }

            AcpiOsPrintf (
                "/**** ACPI table terminates "
                "in the middle of a data structure! */\n");
            return (AE_BAD_DATA);
        }

        if (Info->Opcode == ACPI_DMT_EXTRA_TEXT)
        {
            AcpiOsPrintf ("%s", Info->Name);
            continue;
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

        case ACPI_DMT_FLAGS1:

            AcpiOsPrintf ("%1.1X\n", (*Target >> 1) & 0x03);
            break;

        case ACPI_DMT_FLAGS2:

            AcpiOsPrintf ("%1.1X\n", (*Target >> 2) & 0x03);
            break;

        case ACPI_DMT_FLAGS8_2:

            AcpiOsPrintf ("%2.2X\n", (*Target >> 2) & 0xFF);
            break;

	case ACPI_DMT_FLAGS4:

            AcpiOsPrintf ("%1.1X\n", (*Target >> 4) & 0x03);
            break;

        case ACPI_DMT_FLAGS4_0:

            AcpiOsPrintf ("%1.1X\n", (*(UINT32 *)Target) & 0x0F);
            break;

        case ACPI_DMT_FLAGS4_4:

            AcpiOsPrintf ("%1.1X\n", (*(UINT32 *)Target >> 4) & 0x0F);
            break;

        case ACPI_DMT_FLAGS4_8:

            AcpiOsPrintf ("%1.1X\n", (*(UINT32 *)Target >> 8) & 0x0F);
            break;

        case ACPI_DMT_FLAGS4_12:

            AcpiOsPrintf ("%1.1X\n", (*(UINT32 *)Target >> 12) & 0x0F);
            break;

        case ACPI_DMT_FLAGS16_16:

            AcpiOsPrintf ("%4.4X\n", (*(UINT32 *)Target >> 16) & 0xFFFF);
            break;

        /* Integer Data Types */

        case ACPI_DMT_UINT8:
        case ACPI_DMT_UINT16:
        case ACPI_DMT_UINT24:
        case ACPI_DMT_UINT32:
        case ACPI_DMT_UINT40:
        case ACPI_DMT_UINT48:
        case ACPI_DMT_UINT56:
        case ACPI_DMT_UINT64:
            /*
             * Dump bytes - high byte first, low byte last.
             * Note: All ACPI tables are little-endian.
             */
            Value = 0;
            for (Temp8 = (UINT8) ByteLength; Temp8 > 0; Temp8--)
            {
                AcpiOsPrintf ("%2.2X", Target[Temp8 - 1]);
                Value |= Target[Temp8 - 1];
                Value <<= 8;
            }

            if (!Value && (Info->Flags & DT_DESCRIBES_OPTIONAL))
            {
                AcpiOsPrintf (" [Optional field not present]");
            }

            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_BUF7:
        case ACPI_DMT_BUF10:
        case ACPI_DMT_BUF12:
        case ACPI_DMT_BUF16:
        case ACPI_DMT_BUF18:
        case ACPI_DMT_BUF128:
            /*
             * Buffer: Size depends on the opcode and was set above.
             * Each hex byte is separated with a space.
             * Multiple lines are separated by line continuation char.
             */
            for (Temp16 = 0; Temp16 < ByteLength; Temp16++)
            {
                AcpiOsPrintf ("%2.2X", Target[Temp16]);
                if ((UINT32) (Temp16 + 1) < ByteLength)
                {
                    if ((Temp16 > 0) && (!((Temp16+1) % 16)))
                    {
                        AcpiOsPrintf (" \\\n"); /* Line continuation */
                        AcpiDmLineHeader (0, 0, NULL);
                    }
                    else
                    {
                        AcpiOsPrintf (" ");
                    }
                }
            }

            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_UUID:

            /* Convert 16-byte UUID buffer to 36-byte formatted UUID string */

            (void) AcpiUtConvertUuidToString ((char *) Target, AslGbl_MsgBuffer);

            AcpiOsPrintf ("%s\n", AslGbl_MsgBuffer);
            break;

        case ACPI_DMT_STRING:

            AcpiOsPrintf ("\"%s\"\n", ACPI_CAST_PTR (char, Target));
            break;

        case ACPI_DMT_IVRS_UNTERMINATED_STRING:

            AcpiOsPrintf ("\"%.*s\"\n", ByteLength, ACPI_CAST_PTR (char, Target));
            break;

        /* Fixed length ASCII name fields */

        case ACPI_DMT_SIG:

            AcpiUtCheckAndRepairAscii (Target, RepairedName, 4);
            AcpiOsPrintf ("\"%.4s\"    ", RepairedName);

            TableData = AcpiAhGetTableInfo (ACPI_CAST_PTR (char, Target));
            if (TableData)
            {
                AcpiOsPrintf (STRING_FORMAT, TableData->Description);
            }
            else
            {
                AcpiOsPrintf ("\n");
            }
            break;

        case ACPI_DMT_NAME4:

            AcpiUtCheckAndRepairAscii (Target, RepairedName, 4);
            AcpiOsPrintf ("\"%.4s\"\n", RepairedName);
            break;

        case ACPI_DMT_NAME6:

            AcpiUtCheckAndRepairAscii (Target, RepairedName, 6);
            AcpiOsPrintf ("\"%.6s\"\n", RepairedName);
            break;

        case ACPI_DMT_NAME8:

            AcpiUtCheckAndRepairAscii (Target, RepairedName, 8);
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

            AcpiOsPrintf (UINT8_FORMAT, *Target, AcpiUtGetRegionName (*Target));
            break;

        case ACPI_DMT_ACCWIDTH:

            /* Encoded Access Width */

            Temp8 = *Target;
            if (Temp8 > ACPI_GAS_WIDTH_RESERVED)
            {
                Temp8 = ACPI_GAS_WIDTH_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target, AcpiDmGasAccessWidth[Temp8]);
            break;

        case ACPI_DMT_GAS:

            /* Generic Address Structure */

            AcpiOsPrintf (STRING_FORMAT, "Generic Address Structure");
            Status = AcpiDmDumpTable (TableLength, CurrentOffset, Target,
                sizeof (ACPI_GENERIC_ADDRESS), AcpiDmTableInfoGas);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            AcpiOsPrintf ("\n");
            LastOutputBlankLine = TRUE;
            break;

        case ACPI_DMT_AEST:

            /* AEST subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_AEST_NODE_TYPE_RESERVED)
            {
                Temp8 = ACPI_AEST_NODE_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmAestSubnames[Temp8]);
            break;

        case ACPI_DMT_AEST_CACHE:

            /* AEST cache resource subtable */

            Temp32 = *Target;
            if (Temp32 > ACPI_AEST_CACHE_RESERVED)
            {
                Temp32 = ACPI_AEST_CACHE_RESERVED;
            }

            AcpiOsPrintf (UINT32_FORMAT, *Target,
                AcpiDmAestCacheNames[Temp32]);
            break;

        case ACPI_DMT_AEST_GIC:

            /* AEST GIC error subtable */

            Temp32 = *Target;
            if (Temp32 > ACPI_AEST_GIC_RESERVED)
            {
                Temp32 = ACPI_AEST_GIC_RESERVED;
            }

            AcpiOsPrintf (UINT32_FORMAT, *Target,
                AcpiDmAestGicNames[Temp32]);
            break;

        case ACPI_DMT_AEST_RES:

            /* AEST resource type subtable */

            Temp8 = *Target;
            if (Temp8 > ACPI_AEST_RESOURCE_RESERVED)
            {
                Temp8 = ACPI_AEST_RESOURCE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmAestResourceNames[Temp8]);
            break;

        case ACPI_DMT_AEST_XFACE:

            /* AEST interface structure types */

            Temp8 = *Target;
            if (Temp8 > ACPI_AEST_XFACE_RESERVED)
            {
                Temp8 = ACPI_AEST_XFACE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmAestXfaceNames[Temp8]);
            break;

        case ACPI_DMT_AEST_XRUPT:

            /* AEST interrupt structure types */

            Temp8 = *Target;
            if (Temp8 > ACPI_AEST_XRUPT_RESERVED)
            {
                Temp8 = ACPI_AEST_XRUPT_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmAestXruptNames[Temp8]);
            break;

        case ACPI_DMT_ASF:

            /* ASF subtable types */

            Temp16 = (UINT16) ((*Target) & 0x7F);  /* Top bit can be zero or one */
            if (Temp16 > ACPI_ASF_TYPE_RESERVED)
            {
                Temp16 = ACPI_ASF_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target, AcpiDmAsfSubnames[Temp16]);
            break;

        case ACPI_DMT_CEDT:

            /* CEDT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_CEDT_TYPE_RESERVED)
            {
                Temp8 = ACPI_CEDT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmCedtSubnames[Temp8]);
            break;

        case ACPI_DMT_DMAR:

            /* DMAR subtable types */

            Temp16 = ACPI_GET16 (Target);
            if (Temp16 > ACPI_DMAR_TYPE_RESERVED)
            {
                Temp16 = ACPI_DMAR_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT16_FORMAT, ACPI_GET16 (Target),
                AcpiDmDmarSubnames[Temp16]);
            break;

        case ACPI_DMT_DMAR_SCOPE:

            /* DMAR device scope types */

            Temp8 = *Target;
            if (Temp8 > ACPI_DMAR_SCOPE_TYPE_RESERVED)
            {
                Temp8 = ACPI_DMAR_SCOPE_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmDmarScope[Temp8]);
            break;

        case ACPI_DMT_EINJACT:

            /* EINJ Action types */

            Temp8 = *Target;
            if (Temp8 > ACPI_EINJ_ACTION_RESERVED)
            {
                Temp8 = ACPI_EINJ_ACTION_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmEinjActions[Temp8]);
            break;

        case ACPI_DMT_EINJINST:

            /* EINJ Instruction types */

            Temp8 = *Target;
            if (Temp8 > ACPI_EINJ_INSTRUCTION_RESERVED)
            {
                Temp8 = ACPI_EINJ_INSTRUCTION_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmEinjInstructions[Temp8]);
            break;

        case ACPI_DMT_ERSTACT:

            /* ERST Action types */

            Temp8 = *Target;
            if (Temp8 > ACPI_ERST_ACTION_RESERVED)
            {
                Temp8 = ACPI_ERST_ACTION_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmErstActions[Temp8]);
            break;

        case ACPI_DMT_ERSTINST:

            /* ERST Instruction types */

            Temp8 = *Target;
            if (Temp8 > ACPI_ERST_INSTRUCTION_RESERVED)
            {
                Temp8 = ACPI_ERST_INSTRUCTION_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmErstInstructions[Temp8]);
            break;

        case ACPI_DMT_GTDT:

            /* GTDT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_GTDT_TYPE_RESERVED)
            {
                Temp8 = ACPI_GTDT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmGtdtSubnames[Temp8]);
            break;

        case ACPI_DMT_HEST:

            /* HEST subtable types */

            Temp16 = ACPI_GET16 (Target);
            if (Temp16 > ACPI_HEST_TYPE_RESERVED)
            {
                Temp16 = ACPI_HEST_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT16_FORMAT, ACPI_GET16 (Target),
                AcpiDmHestSubnames[Temp16]);
            break;

        case ACPI_DMT_HESTNTFY:

            AcpiOsPrintf (STRING_FORMAT,
                "Hardware Error Notification Structure");

            Status = AcpiDmDumpTable (TableLength, CurrentOffset, Target,
                sizeof (ACPI_HEST_NOTIFY), AcpiDmTableInfoHestNotify);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

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

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmHestNotifySubnames[Temp8]);
            break;

        case ACPI_DMT_HMAT:

            /* HMAT subtable types */

            Temp16 = *Target;
            if (Temp16 > ACPI_HMAT_TYPE_RESERVED)
            {
                Temp16 = ACPI_HMAT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT16_FORMAT, *Target,
                AcpiDmHmatSubnames[Temp16]);
            break;

        case ACPI_DMT_IORTMEM:

            AcpiOsPrintf (STRING_FORMAT,
                "IORT Memory Access Properties");

            Status = AcpiDmDumpTable (TableLength, CurrentOffset, Target,
                sizeof (ACPI_IORT_MEMORY_ACCESS), AcpiDmTableInfoIortAcc);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            LastOutputBlankLine = TRUE;
            break;

        case ACPI_DMT_MADT:

            /* MADT subtable types */

            Temp8 = *Target;
            if ((Temp8 >= ACPI_MADT_TYPE_RESERVED) && (Temp8 < ACPI_MADT_TYPE_OEM_RESERVED))
            {
                Temp8 = ACPI_MADT_TYPE_RESERVED;
            }
            else if (Temp8 >= ACPI_MADT_TYPE_OEM_RESERVED)
            {
                Temp8 = ACPI_MADT_TYPE_RESERVED + 1;
            }
            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmMadtSubnames[Temp8]);
            break;

        case ACPI_DMT_NFIT:

            /* NFIT subtable types */

            Temp16 = ACPI_GET16 (Target);
            if (Temp16 > ACPI_NFIT_TYPE_RESERVED)
            {
                Temp16 = ACPI_NFIT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT16_FORMAT, ACPI_GET16 (Target),
                AcpiDmNfitSubnames[Temp16]);
            break;

        case ACPI_DMT_NHLT1:

            /* NHLT link types */

            Temp8 = *Target;
            if (Temp8 > ACPI_NHLT_TYPE_RESERVED)
            {
                Temp8 = ACPI_NHLT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmNhltLinkTypeNames[Temp8]);
            break;

        case ACPI_DMT_NHLT1a:

            /* NHLT direction */

            Temp8 = *Target;
            if (Temp8 > ACPI_NHLT_DIR_RESERVED)
            {
                Temp8 = ACPI_NHLT_DIR_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmNhltDirectionNames[Temp8]);
            break;

        case ACPI_DMT_NHLT1b:

            /* NHLT microphone type */

            Temp8 = *Target;
            if (Temp8 > ACPI_NHLT_MIC_RESERVED)
            {
                Temp8 = ACPI_NHLT_MIC_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmNhltMicTypeNames[Temp8]);
            break;

        case ACPI_DMT_NHLT1c:

            /* NHLT microphone position */

            Temp8 = *Target;
            if (Temp8 > ACPI_NHLT_MIC_POSITION_RESERVED)
            {
                Temp8 = ACPI_NHLT_MIC_POSITION_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmNhltMicPositionNames[Temp8]);
            break;

        case ACPI_DMT_NHLT1d:

            /* NHLT microphone array type */

            Temp8 = *Target & ACPI_NHLT_ARRAY_TYPE_MASK;
            if (Temp8 < ACPI_NHLT_ARRAY_TYPE_RESERVED)
            {
                Temp8 = ACPI_NHLT_ARRAY_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT_NO_NEWLINE, *Target,
                AcpiDmNhltMicArrayTypeNames[Temp8 - ACPI_NHLT_ARRAY_TYPE_RESERVED]);

            Temp8 = *Target;
            if (Temp8 & ACPI_NHLT_MIC_SNR_SENSITIVITY_EXT)
            {
                AcpiOsPrintf (" [%s]", "SNR and Sensitivity");
            }

            AcpiOsPrintf ("\n");
            break;

        case ACPI_DMT_NHLT1e:

            /* NHLT Endpoint Device ID */

            Temp16 = ACPI_GET16 (Target);
            if (Temp16 == 0xAE20)
            {
                Name = "PDM DMIC";
            }
            else if (Temp16 == 0xAE30)
            {
                Name = "BT Sideband";
            }
            else if (Temp16 == 0xAE34)
            {
                Name = "I2S/TDM Codecs";
            }
            else
            {
                Name = "Unknown Device ID";
            }

            AcpiOsPrintf (UINT16_FORMAT, Temp16, Name);
            break;

        case ACPI_DMT_NHLT1f:

            /* NHLT ConfigType field */

            Temp8 = *Target;
            if (Temp8 > ACPI_NHLT_CONFIG_TYPE_RESERVED)
            {
                Temp8 = ACPI_NHLT_CONFIG_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmNhltConfigTypeNames[Temp8]);
            break;

        case ACPI_DMT_PCCT:

            /* PCCT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_PCCT_TYPE_RESERVED)
            {
                Temp8 = ACPI_PCCT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmPcctSubnames[Temp8]);
            break;

        case ACPI_DMT_PHAT:

            /* PMTT subtable types */

            Temp16 = *Target;
            if (Temp16 > ACPI_PHAT_TYPE_RESERVED)
            {
                Temp16 = ACPI_PHAT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT16_FORMAT, ACPI_GET16(Target),
                AcpiDmPhatSubnames[Temp16]);
            break;

        case ACPI_DMT_PMTT:

            /* PMTT subtable types */

            Temp8 = *Target;
            if (Temp8 == ACPI_PMTT_TYPE_VENDOR)
            {
                Temp8 = ACPI_PMTT_TYPE_RESERVED + 1;
            }
            else if (Temp8 > ACPI_PMTT_TYPE_RESERVED)
            {
                Temp8 = ACPI_PMTT_TYPE_RESERVED;
            }
            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmPmttSubnames[Temp8]);
            break;

        case ACPI_DMT_PPTT:

            /* PPTT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_PPTT_TYPE_RESERVED)
            {
                Temp8 = ACPI_PPTT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmPpttSubnames[Temp8]);
            break;

        case ACPI_DMT_UNICODE:
        case ACPI_DMT_WPBT_UNICODE:

            if (ByteLength == 0)
            {
                AcpiOsPrintf ("/* Zero-length Data */\n");
                break;
            }

            AcpiDmDumpUnicode (Table, CurrentOffset, ByteLength);
            break;

        case ACPI_DMT_RAW_BUFFER:
        case ACPI_DMT_BUFFER:
        case ACPI_DMT_PMTT_VENDOR:

            if (ByteLength == 0)
            {
                AcpiOsPrintf ("/* Zero-length Data */\n");
                break;
            }

            AcpiDmDumpBuffer (Target, 0, ByteLength, CurrentOffset, NULL);
            break;

        case ACPI_DMT_RGRT:

            /* RGRT subtable types */

            Temp8 = *Target;
            if (Temp8 >= ACPI_RGRT_TYPE_RESERVED)
            {
                Temp8 = ACPI_RGRT_TYPE_RESERVED0;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmRgrtSubnames[Temp8]);
            break;

        case ACPI_DMT_SDEV:

            /* SDEV subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_SDEV_TYPE_RESERVED)
            {
                Temp8 = ACPI_SDEV_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmSdevSubnames[Temp8]);
            break;

        case ACPI_DMT_SRAT:

            /* SRAT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_SRAT_TYPE_RESERVED)
            {
                Temp8 = ACPI_SRAT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmSratSubnames[Temp8]);
            break;

        case ACPI_DMT_TPM2:

            /* TPM2 Start Method types */

            Temp8 = *Target;
            if (Temp8 > ACPI_TPM2_RESERVED)
            {
                Temp8 = ACPI_TPM2_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmTpm2Subnames[Temp8]);
            break;


        case ACPI_DMT_FADTPM:

            /* FADT Preferred PM Profile names */

            Temp8 = *Target;
            if (Temp8 > ACPI_FADT_PM_RESERVED)
            {
                Temp8 = ACPI_FADT_PM_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmFadtProfiles[Temp8]);
            break;

        case ACPI_DMT_IVRS:

            /* IVRS subtable types */

            Temp8 = *Target;
            switch (Temp8)
            {
            case ACPI_IVRS_TYPE_HARDWARE1:
            case ACPI_IVRS_TYPE_HARDWARE2:

                Name = AcpiDmIvrsSubnames[0];
                break;

            case ACPI_IVRS_TYPE_HARDWARE3:

                Name = AcpiDmIvrsSubnames[1];
                break;

            case ACPI_IVRS_TYPE_MEMORY1:
            case ACPI_IVRS_TYPE_MEMORY2:
            case ACPI_IVRS_TYPE_MEMORY3:

                Name = AcpiDmIvrsSubnames[2];
                break;

            default:

                Name = AcpiDmIvrsSubnames[3];
                break;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target, Name);
            break;

        case ACPI_DMT_IVRS_DE:

            /* IVRS device entry types */

            Temp8 = *Target;
            switch (Temp8)
            {
            case ACPI_IVRS_TYPE_ALL:
            case ACPI_IVRS_TYPE_SELECT:
            case ACPI_IVRS_TYPE_START:
            case ACPI_IVRS_TYPE_END:

                Name = AcpiDmIvrsDevEntryNames[Temp8];
                break;

            case ACPI_IVRS_TYPE_ALIAS_SELECT:
            case ACPI_IVRS_TYPE_ALIAS_START:
            case ACPI_IVRS_TYPE_EXT_SELECT:
            case ACPI_IVRS_TYPE_EXT_START:
            case ACPI_IVRS_TYPE_SPECIAL:

                Name = AcpiDmIvrsDevEntryNames[Temp8 - 61];
                break;

            case ACPI_IVRS_TYPE_HID:

                Name = AcpiDmIvrsDevEntryNames[Temp8 - 228];
                break;

            default:
                Name = AcpiDmIvrsDevEntryNames[0];  /* Unknown/Reserved */
                break;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target, Name);
            break;

        case ACPI_DMT_LPIT:

            /* LPIT subtable types */

            Temp32 = ACPI_GET32 (Target);
            if (Temp32 > ACPI_LPIT_TYPE_RESERVED)
            {
                Temp32 = ACPI_LPIT_TYPE_RESERVED;
            }

            AcpiOsPrintf (UINT32_FORMAT, ACPI_GET32 (Target),
                AcpiDmLpitSubnames[Temp32]);
            break;

        case ACPI_DMT_VIOT:

            /* VIOT subtable types */

            Temp8 = *Target;
            if (Temp8 > ACPI_VIOT_RESERVED)
            {
                Temp8 = ACPI_VIOT_RESERVED;
            }

            AcpiOsPrintf (UINT8_FORMAT, *Target,
                AcpiDmViotSubnames[Temp8]);
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
        /*
         * If this table is not the main table, the subtable must have a
         * valid length
         */
        AcpiOsPrintf ("Invalid zero length subtable\n");
        return (AE_BAD_DATA);
    }

    return (AE_OK);
}
