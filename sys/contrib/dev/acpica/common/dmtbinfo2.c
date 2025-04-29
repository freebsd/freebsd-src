/******************************************************************************
 *
 * Module Name: dmtbinfo2 - Table info for non-AML tables
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2025, Intel Corp.
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
#include <contrib/dev/acpica/include/actbinfo.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtbinfo2")

/*
 * How to add a new table:
 *
 * - Add the C table definition to the actbl1.h or actbl2.h header.
 * - Add ACPI_xxxx_OFFSET macro(s) for the table (and subtables) to list below.
 * - Define the table in this file (for the disassembler). If any
 *   new data types are required (ACPI_DMT_*), see below.
 * - Add an external declaration for the new table definition (AcpiDmTableInfo*)
 *     in acdisam.h
 * - Add new table definition to the dispatch table in dmtable.c (AcpiDmTableData)
 *     If a simple table (with no subtables), no disassembly code is needed.
 *     Otherwise, create the AcpiDmDump* function for to disassemble the table
 *     and add it to the dmtbdump.c file.
 * - Add an external declaration for the new AcpiDmDump* function in acdisasm.h
 * - Add the new AcpiDmDump* function to the dispatch table in dmtable.c
 * - Create a template for the new table
 * - Add data table compiler support
 *
 * How to add a new data type (ACPI_DMT_*):
 *
 * - Add new type at the end of the ACPI_DMT list in acdisasm.h
 * - Add length and implementation cases in dmtable.c  (disassembler)
 * - Add type and length cases in dtutils.c (DT compiler)
 */

/*
 * Remaining tables are not consumed directly by the ACPICA subsystem
 */

/*******************************************************************************
 *
 * AGDI - Arm Generic Diagnostic Dump and Reset Device Interface
 *
 * Conforms to "ACPI for Arm Components 1.1, Platform Design Document"
 * ARM DEN0093 v1.1
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoAgdi[] =
{
    {ACPI_DMT_UINT8,    ACPI_AGDI_OFFSET (Flags),                   "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_AGDI_FLAG_OFFSET (Flags, 0),           "Signalling mode", 0},
    {ACPI_DMT_UINT24,   ACPI_AGDI_OFFSET (Reserved[0]),             "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_AGDI_OFFSET (SdeiEvent),               "SdeiEvent", 0},
    {ACPI_DMT_UINT32,   ACPI_AGDI_OFFSET (Gsiv),                    "Gsiv", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * APMT - ARM Performance Monitoring Unit Table
 *
 * Conforms to:
 * ARM Performance Monitoring Unit Architecture 1.0 Platform Design Document
 * ARM DEN0117 v1.0 November 25, 2021
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoApmtNode[] =
{
    {ACPI_DMT_UINT16,  ACPI_APMTN_OFFSET (Length),                  "Length of APMT Node", 0},
    {ACPI_DMT_UINT8,   ACPI_APMTN_OFFSET (Flags),                   "Node Flags", 0},
    {ACPI_DMT_FLAG0,   ACPI_APMTN_FLAG_OFFSET (Flags, 0),           "Dual Page Extension", 0},
    {ACPI_DMT_FLAG1,   ACPI_APMTN_FLAG_OFFSET (Flags, 0),           "Processor Affinity Type", 0},
    {ACPI_DMT_FLAG2,   ACPI_APMTN_FLAG_OFFSET (Flags, 0),           "64-bit Atomic Support", 0},
    {ACPI_DMT_UINT8,   ACPI_APMTN_OFFSET (Type),                    "Node Type", 0},
    {ACPI_DMT_UINT32,  ACPI_APMTN_OFFSET (Id),                      "Unique Node Identifier", 0},
    {ACPI_DMT_UINT64,  ACPI_APMTN_OFFSET (InstPrimary),             "Primary Node Instance", 0},
    {ACPI_DMT_UINT32,  ACPI_APMTN_OFFSET (InstSecondary),           "Secondary Node Instance", 0},
    {ACPI_DMT_UINT64,  ACPI_APMTN_OFFSET (BaseAddress0),            "Page 0 Base Address", 0},
    {ACPI_DMT_UINT64,  ACPI_APMTN_OFFSET (BaseAddress1),            "Page 1 Base Address", 0},
    {ACPI_DMT_UINT32,  ACPI_APMTN_OFFSET (OvflwIrq),                "Overflow Interrupt ID", 0},
    {ACPI_DMT_UINT32,  ACPI_APMTN_OFFSET (Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT32,  ACPI_APMTN_OFFSET (OvflwIrqFlags),           "Overflow Interrupt Flags", 0},
    {ACPI_DMT_FLAG0,   ACPI_APMTN_FLAG_OFFSET (OvflwIrqFlags, 0),   "Interrupt Mode", 0},
    {ACPI_DMT_FLAG1,   ACPI_APMTN_FLAG_OFFSET (OvflwIrqFlags, 0),   "Interrupt Type", 0},
    {ACPI_DMT_UINT32,  ACPI_APMTN_OFFSET (ProcAffinity),            "Processor Affinity", 0},
    {ACPI_DMT_UINT32,  ACPI_APMTN_OFFSET (ImplId),                  "Implementation ID", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * IORT - IO Remapping Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort[] =
{
    {ACPI_DMT_UINT32,   ACPI_IORT_OFFSET (NodeCount),               "Node Count", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT_OFFSET (NodeOffset),              "Node Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* Optional padding field */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIortPad[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "Optional Padding", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIortHdr[] =
{
    {ACPI_DMT_UINT8,    ACPI_IORTH_OFFSET (Type),                   "Type", 0},
    {ACPI_DMT_UINT16,   ACPI_IORTH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_IORTH_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTH_OFFSET (Identifier),             "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTH_OFFSET (MappingCount),           "Mapping Count", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTH_OFFSET (MappingOffset),          "Mapping Offset", 0},
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header (one per Subtable)- Revision 3 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIortHdr3[] =
{
    {ACPI_DMT_UINT8,    ACPI_IORTH_OFFSET (Type),                   "Type", 0},
    {ACPI_DMT_UINT16,   ACPI_IORTH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_IORTH_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTH_OFFSET (Identifier),             "Identifier", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTH_OFFSET (MappingCount),           "Mapping Count", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTH_OFFSET (MappingOffset),          "Mapping Offset", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIortMap[] =
{
    {ACPI_DMT_UINT32,   ACPI_IORTM_OFFSET (InputBase),              "Input base", DT_OPTIONAL},
    {ACPI_DMT_UINT32,   ACPI_IORTM_OFFSET (IdCount),                "ID Count", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTM_OFFSET (OutputBase),             "Output Base", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTM_OFFSET (OutputReference),        "Output Reference", 0},
    {ACPI_DMT_UINT32,   ACPI_IORTM_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORTM_FLAG_OFFSET (Flags, 0),          "Single Mapping", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIortAcc[] =
{
    {ACPI_DMT_UINT32,   ACPI_IORTA_OFFSET (CacheCoherency),         "Cache Coherency", 0},
    {ACPI_DMT_UINT8,    ACPI_IORTA_OFFSET (Hints),                  "Hints (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORTA_FLAG_OFFSET (Hints, 0),          "Transient", 0},
    {ACPI_DMT_FLAG1,    ACPI_IORTA_FLAG_OFFSET (Hints, 0),          "Write Allocate", 0},
    {ACPI_DMT_FLAG2,    ACPI_IORTA_FLAG_OFFSET (Hints, 0),          "Read Allocate", 0},
    {ACPI_DMT_FLAG3,    ACPI_IORTA_FLAG_OFFSET (Hints, 0),          "Override", 0},
    {ACPI_DMT_UINT16,   ACPI_IORTA_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_IORTA_OFFSET (MemoryFlags),            "Memory Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORTA_FLAG_OFFSET (MemoryFlags, 0),    "Coherency", 0},
    {ACPI_DMT_FLAG1,    ACPI_IORTA_FLAG_OFFSET (MemoryFlags, 0),    "Device Attribute", 0},
    {ACPI_DMT_FLAG2,    ACPI_IORTA_FLAG_OFFSET (MemoryFlags, 0),    "Ensured Coherency of Accesses", 0},
    ACPI_DMT_TERMINATOR
};

/* IORT subtables */

/* 0x00: ITS Group */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort0[] =
{
    {ACPI_DMT_UINT32,   ACPI_IORT0_OFFSET (ItsCount),               "ItsCount", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort0a[] =
{
    {ACPI_DMT_UINT32,   0,                                          "Identifiers", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 0x01: Named Component */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort1[] =
{
    {ACPI_DMT_UINT32,   ACPI_IORT1_OFFSET (NodeFlags),              "Node Flags", 0},
    {ACPI_DMT_IORTMEM,  ACPI_IORT1_OFFSET (MemoryProperties),       "Memory Properties", 0},
    {ACPI_DMT_UINT8,    ACPI_IORT1_OFFSET (MemoryAddressLimit),     "Memory Size Limit", 0},
    {ACPI_DMT_STRING,   ACPI_IORT1_OFFSET (DeviceName[0]),          "Device Name", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort1a[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "Padding", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 0x02: PCI Root Complex */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort2[] =
{
    {ACPI_DMT_IORTMEM,  ACPI_IORT2_OFFSET (MemoryProperties),       "Memory Properties", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT2_OFFSET (AtsAttribute),           "ATS Attribute", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT2_OFFSET (PciSegmentNumber),       "PCI Segment Number", 0},
    {ACPI_DMT_UINT8,    ACPI_IORT2_OFFSET (MemoryAddressLimit),     "Memory Size Limit", 0},
    {ACPI_DMT_UINT16,   ACPI_IORT2_OFFSET (PasidCapabilities),      "PASID Capabilities", 0},
    {ACPI_DMT_UINT8,    ACPI_IORT2_OFFSET (Reserved[0]),            "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 0x03: SMMUv1/2 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort3[] =
{
    {ACPI_DMT_UINT64,   ACPI_IORT3_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_IORT3_OFFSET (Span),                   "Span", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3_OFFSET (Model),                  "Model", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORT3_FLAG_OFFSET (Flags, 0),          "DVM Supported", 0},
    {ACPI_DMT_FLAG1,    ACPI_IORT3_FLAG_OFFSET (Flags, 0),          "Coherent Walk", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3_OFFSET (GlobalInterruptOffset),  "Global Interrupt Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3_OFFSET (ContextInterruptCount),  "Context Interrupt Count", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3_OFFSET (ContextInterruptOffset), "Context Interrupt Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3_OFFSET (PmuInterruptCount),      "PMU Interrupt Count", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3_OFFSET (PmuInterruptOffset),     "PMU Interrupt Offset", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort3a[] =
{
    {ACPI_DMT_UINT32,   ACPI_IORT3A_OFFSET (NSgIrpt),                   "NSgIrpt", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3A_OFFSET (NSgIrptFlags),              "NSgIrpt Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORT3a_FLAG_OFFSET (NSgIrptFlags, 0),      "Edge Triggered", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3A_OFFSET (NSgCfgIrpt),                "NSgCfgIrpt", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT3A_OFFSET (NSgCfgIrptFlags),           "NSgCfgIrpt Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORT3a_FLAG_OFFSET (NSgCfgIrptFlags, 0),   "Edge Triggered", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort3b[] =
{
    {ACPI_DMT_UINT64,   0,                                          "Context Interrupt", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort3c[] =
{
    {ACPI_DMT_UINT64,   0,                                          "PMU Interrupt", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 0x04: SMMUv3 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort4[] =
{
    {ACPI_DMT_UINT64,   ACPI_IORT4_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORT4_FLAG_OFFSET (Flags, 0),          "COHACC Override", 0},
    {ACPI_DMT_FLAG1,    ACPI_IORT4_FLAG_OFFSET (Flags, 0),          "HTTU Override", 0},
    {ACPI_DMT_FLAG3,    ACPI_IORT4_FLAG_OFFSET (Flags, 0),          "Proximity Domain Valid", 0},
    {ACPI_DMT_FLAG4,    ACPI_IORT4_FLAG_OFFSET (Flags, 0),          "DeviceID Valid", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_IORT4_OFFSET (VatosAddress),           "VATOS Address", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (Model),                  "Model", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (EventGsiv),              "Event GSIV", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (PriGsiv),                "PRI GSIV", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (GerrGsiv),               "GERR GSIV", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (SyncGsiv),               "Sync GSIV", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (Pxm),                    "Proximity Domain", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT4_OFFSET (IdMappingIndex),         "Device ID Mapping Index", 0},
    ACPI_DMT_TERMINATOR
};

/* 0x05: PMCG */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort5[] =
{
    {ACPI_DMT_UINT64,   ACPI_IORT5_OFFSET (Page0BaseAddress),       "Page 0 Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT5_OFFSET (OverflowGsiv),           "Overflow Interrupt GSIV", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT5_OFFSET (NodeReference),          "Node Reference", 0},
    {ACPI_DMT_UINT64,   ACPI_IORT5_OFFSET (Page1BaseAddress),       "Page 1 Base Address", 0},
    ACPI_DMT_TERMINATOR
};


/* 0x06: RMR */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort6[] =
{
    {ACPI_DMT_UINT32,   ACPI_IORT6_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_IORT6_FLAG_OFFSET (Flags, 0),          "Remapping Permitted", 0},
    {ACPI_DMT_FLAG1,    ACPI_IORT6_FLAG_OFFSET (Flags, 0),          "Access Privileged", 0},
    {ACPI_DMT_FLAGS8_2, ACPI_IORT6_FLAG_OFFSET (Flags, 0),          "Access Attributes", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT6_OFFSET (RmrCount),               "Number of RMR Descriptors", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT6_OFFSET (RmrOffset),              "RMR Descriptor Offset", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIort6a[] =
{
    {ACPI_DMT_UINT64,   ACPI_IORT6A_OFFSET (BaseAddress),           "Base Address of RMR", DT_OPTIONAL},
    {ACPI_DMT_UINT64,   ACPI_IORT6A_OFFSET (Length),                "Length of RMR", 0},
    {ACPI_DMT_UINT32,   ACPI_IORT6A_OFFSET (Reserved),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/*******************************************************************************
 *
 * IVRS - I/O Virtualization Reporting Structure
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrs[] =
{
    {ACPI_DMT_UINT32,   ACPI_IVRS_OFFSET (Info),                    "Virtualization Info", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* IVRS subtables */

/* 0x10: I/O Virtualization Hardware Definition (IVHD) Block */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsHware1[] =
{
    {ACPI_DMT_IVRS,     ACPI_IVRSH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_IVRSH_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "HtTunEn", 0},
    {ACPI_DMT_FLAG1,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "PassPW", 0},
    {ACPI_DMT_FLAG2,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "ResPassPW", 0},
    {ACPI_DMT_FLAG3,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Isoc Control", 0},
    {ACPI_DMT_FLAG4,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Iotlb Support", 0},
    {ACPI_DMT_FLAG5,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Coherent", 0},
    {ACPI_DMT_FLAG6,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Prefetch Support", 0},
    {ACPI_DMT_FLAG7,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "PPR Support", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRSH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT16,   ACPI_IVRSH_OFFSET (DeviceId),               "DeviceId", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS0_OFFSET (CapabilityOffset),       "Capability Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS0_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS0_OFFSET (PciSegmentGroup),        "PCI Segment Group", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS0_OFFSET (Info),                   "Virtualization Info", 0},
    {ACPI_DMT_UINT32,   ACPI_IVRS0_OFFSET (FeatureReporting),       "Feature Reporting", 0},
    ACPI_DMT_TERMINATOR
};

/* 0x11, 0x40: I/O Virtualization Hardware Definition (IVHD) Block */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsHware23[] =
{
    {ACPI_DMT_IVRS,     ACPI_IVRSH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_IVRSH_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "HtTunEn", 0},
    {ACPI_DMT_FLAG1,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "PassPW", 0},
    {ACPI_DMT_FLAG2,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "ResPassPW", 0},
    {ACPI_DMT_FLAG3,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Isoc Control", 0},
    {ACPI_DMT_FLAG4,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Iotlb Support", 0},
    {ACPI_DMT_FLAG5,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Coherent", 0},
    {ACPI_DMT_FLAG6,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Prefetch Support", 0},
    {ACPI_DMT_FLAG7,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "PPR Support", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS01_OFFSET (Header.Length),         "Length", DT_LENGTH},
    {ACPI_DMT_UINT16,   ACPI_IVRS01_OFFSET (Header.DeviceId),       "DeviceId", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS01_OFFSET (CapabilityOffset),      "Capability Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS01_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS01_OFFSET (PciSegmentGroup),       "PCI Segment Group", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS01_OFFSET (Info),                  "Virtualization Info", 0},
    {ACPI_DMT_UINT32,   ACPI_IVRS01_OFFSET (Attributes),            "Attributes", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS01_OFFSET (EfrRegisterImage),      "EFR Image", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS01_OFFSET (Reserved),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 0x20, 0x21, 0x22: I/O Virtualization Memory Definition (IVMD) Device Entry Block */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsMemory[] =
{
    {ACPI_DMT_IVRS,     ACPI_IVRSH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_IVRSH_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Unity", 0},
    {ACPI_DMT_FLAG1,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Readable", 0},
    {ACPI_DMT_FLAG2,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Writeable", 0},
    {ACPI_DMT_FLAG3,    ACPI_IVRS_FLAG_OFFSET (Flags,0),            "Exclusion Range", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRSH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT16,   ACPI_IVRSH_OFFSET (DeviceId),               "DeviceId", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS1_OFFSET (AuxData),                "Auxiliary Data", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS1_OFFSET (StartAddress),           "Start Address", 0},
    {ACPI_DMT_UINT64,   ACPI_IVRS1_OFFSET (MemoryLength),           "Memory Length", 0},
    ACPI_DMT_TERMINATOR
};

/* Device entry header for IVHD block */

#define ACPI_DMT_IVRS_DE_HEADER \
    {ACPI_DMT_IVRS_DE,  ACPI_IVRSD_OFFSET (Type),                   "Subtable Type", 0}, \
    {ACPI_DMT_UINT16,   ACPI_IVRSD_OFFSET (Id),                     "Device ID", 0}, \
    {ACPI_DMT_UINT8,    ACPI_IVRSD_OFFSET (DataSetting),            "Data Setting (decoded below)", 0}, \
    {ACPI_DMT_FLAG0,    ACPI_IVRSDE_FLAG_OFFSET (DataSetting, 0),   "INITPass", 0}, \
    {ACPI_DMT_FLAG1,    ACPI_IVRSDE_FLAG_OFFSET (DataSetting, 0),   "EIntPass", 0}, \
    {ACPI_DMT_FLAG2,    ACPI_IVRSDE_FLAG_OFFSET (DataSetting, 0),   "NMIPass", 0}, \
    {ACPI_DMT_FLAG3,    ACPI_IVRSDE_FLAG_OFFSET (DataSetting, 0),   "Reserved", 0}, \
    {ACPI_DMT_FLAGS4,   ACPI_IVRSDE_FLAG_OFFSET (DataSetting, 0),   "System MGMT", 0}, \
    {ACPI_DMT_FLAG6,    ACPI_IVRSDE_FLAG_OFFSET (DataSetting, 0),   "LINT0 Pass", 0}, \
    {ACPI_DMT_FLAG7,    ACPI_IVRSDE_FLAG_OFFSET (DataSetting, 0),   "LINT1 Pass", 0}

/* 4-byte device entry (Types 1,2,3,4) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrs4[] =
{
    ACPI_DMT_IVRS_DE_HEADER,
    ACPI_DMT_TERMINATOR
};

/* 8-byte device entry (Type Alias Select, Alias Start of Range) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrs8a[] =
{
    ACPI_DMT_IVRS_DE_HEADER,
    {ACPI_DMT_UINT8,    ACPI_IVRS8A_OFFSET (Reserved1),             "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS8A_OFFSET (UsedId),                "Source Used Device ID", 0},
    {ACPI_DMT_UINT8,    ACPI_IVRS8A_OFFSET (Reserved2),             "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 8-byte device entry (Type Extended Select, Extended Start of Range) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrs8b[] =
{
    ACPI_DMT_IVRS_DE_HEADER,
    {ACPI_DMT_UINT32,   ACPI_IVRS8B_OFFSET (ExtendedData),          "Extended Data", 0},
    ACPI_DMT_TERMINATOR
};

/* 8-byte device entry (Type Special Device) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrs8c[] =
{
    ACPI_DMT_IVRS_DE_HEADER,
    {ACPI_DMT_UINT8,    ACPI_IVRS8C_OFFSET (Handle),                "Handle", 0},
    {ACPI_DMT_UINT16,   ACPI_IVRS8C_OFFSET (UsedId),                "Source Used Device ID", 0},
    {ACPI_DMT_UINT8,    ACPI_IVRS8C_OFFSET (Variety),               "Variety", 0},
    ACPI_DMT_TERMINATOR
};

/* Variable-length Device Entry Type 0xF0 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsHid[] =
{
    ACPI_DMT_IVRS_DE_HEADER,
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsUidString[] =
{
    {ACPI_DMT_UINT8,    0,                                          "UID Format", DT_DESCRIBES_OPTIONAL},
    {ACPI_DMT_UINT8,    1,                                          "UID Length", DT_DESCRIBES_OPTIONAL},
    {ACPI_DMT_IVRS_UNTERMINATED_STRING, 2,                          "UID", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsUidInteger[] =
{
    {ACPI_DMT_UINT8,    0,                                          "UID Format", DT_DESCRIBES_OPTIONAL},
    {ACPI_DMT_UINT8,    1,                                          "UID Length", DT_DESCRIBES_OPTIONAL},
    {ACPI_DMT_UINT64, 2,                                            "UID", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsHidString[] =
{
    {ACPI_DMT_NAME8,        0,                                      "ACPI HID", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsHidInteger[] =
{
    {ACPI_DMT_UINT64,       0,                                      "ACPI HID", 0},
    ACPI_DMT_TERMINATOR
};
ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsCidString[] =
{
    {ACPI_DMT_NAME8,        0,                                      "ACPI CID", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoIvrsCidInteger[] =
{
    {ACPI_DMT_UINT64,       0,                                      "ACPI CID", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * LPIT - Low Power Idle Table
 *
 ******************************************************************************/

/* Main table consists only of the standard ACPI table header */

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoLpitHdr[] =
{
    {ACPI_DMT_LPIT,     ACPI_LPITH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT32,   ACPI_LPITH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT16,   ACPI_LPITH_OFFSET (UniqueId),               "Unique ID", 0},
    {ACPI_DMT_UINT16,   ACPI_LPITH_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_LPITH_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_LPITH_FLAG_OFFSET (Flags, 0),          "State Disabled", 0},
    {ACPI_DMT_FLAG1,    ACPI_LPITH_FLAG_OFFSET (Flags, 0),          "No Counter", 0},
    ACPI_DMT_TERMINATOR
};

/* LPIT Subtables */

/* 0: Native C-state */

ACPI_DMTABLE_INFO           AcpiDmTableInfoLpit0[] =
{
    {ACPI_DMT_GAS,      ACPI_LPIT0_OFFSET (EntryTrigger),           "Entry Trigger", 0},
    {ACPI_DMT_UINT32,   ACPI_LPIT0_OFFSET (Residency),              "Residency", 0},
    {ACPI_DMT_UINT32,   ACPI_LPIT0_OFFSET (Latency),                "Latency", 0},
    {ACPI_DMT_GAS,      ACPI_LPIT0_OFFSET (ResidencyCounter),       "Residency Counter", 0},
    {ACPI_DMT_UINT64,   ACPI_LPIT0_OFFSET (CounterFrequency),       "Counter Frequency", 0},
    ACPI_DMT_TERMINATOR
};
/*******************************************************************************
 *
 * MADT - Multiple APIC Description Table and subtables
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt[] =
{
    {ACPI_DMT_UINT32,   ACPI_MADT_OFFSET (Address),                 "Local Apic Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT_OFFSET (Flags),                   "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT_FLAG_OFFSET (Flags,0),            "PC-AT Compatibility", 0},
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadtHdr[] =
{
    {ACPI_DMT_MADT,     ACPI_MADTH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_MADTH_OFFSET (Length),                 "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* MADT Subtables */

/* 0: processor APIC */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt0[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT0_OFFSET (ProcessorId),            "Processor ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT0_OFFSET (Id),                     "Local Apic ID", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT0_OFFSET (LapicFlags),             "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT0_FLAG_OFFSET (LapicFlags,0),      "Processor Enabled", 0},
    {ACPI_DMT_FLAG1,    ACPI_MADT0_FLAG_OFFSET (LapicFlags,0),      "Runtime Online Capable", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: IO APIC */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt1[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT1_OFFSET (Id),                     "I/O Apic ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT1_OFFSET (Address),                "Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT1_OFFSET (GlobalIrqBase),          "Interrupt", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: Interrupt Override */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt2[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT2_OFFSET (Bus),                    "Bus", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT2_OFFSET (SourceIrq),              "Source", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT2_OFFSET (GlobalIrq),              "Interrupt", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT2_OFFSET (IntiFlags),              "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAGS0,   ACPI_MADT2_FLAG_OFFSET (IntiFlags,0),       "Polarity", 0},
    {ACPI_DMT_FLAGS2,   ACPI_MADT2_FLAG_OFFSET (IntiFlags,0),       "Trigger Mode", 0},
    ACPI_DMT_TERMINATOR
};

/* 3: NMI Sources */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt3[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT3_OFFSET (IntiFlags),              "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAGS0,   ACPI_MADT3_FLAG_OFFSET (IntiFlags,0),       "Polarity", 0},
    {ACPI_DMT_FLAGS2,   ACPI_MADT3_FLAG_OFFSET (IntiFlags,0),       "Trigger Mode", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT3_OFFSET (GlobalIrq),              "Interrupt", 0},
    ACPI_DMT_TERMINATOR
};

/* 4: Local APIC NMI */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt4[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT4_OFFSET (ProcessorId),            "Processor ID", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT4_OFFSET (IntiFlags),              "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAGS0,   ACPI_MADT4_FLAG_OFFSET (IntiFlags,0),       "Polarity", 0},
    {ACPI_DMT_FLAGS2,   ACPI_MADT4_FLAG_OFFSET (IntiFlags,0),       "Trigger Mode", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT4_OFFSET (Lint),                   "Interrupt Input LINT", 0},
    ACPI_DMT_TERMINATOR
};

/* 5: Address Override */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt5[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT5_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT5_OFFSET (Address),                "APIC Address", 0},
    ACPI_DMT_TERMINATOR
};

/* 6: I/O Sapic */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt6[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT6_OFFSET (Id),                     "I/O Sapic ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT6_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT6_OFFSET (GlobalIrqBase),          "Interrupt Base", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT6_OFFSET (Address),                "Address", 0},
    ACPI_DMT_TERMINATOR
};

/* 7: Local Sapic */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt7[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT7_OFFSET (ProcessorId),            "Processor ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT7_OFFSET (Id),                     "Local Sapic ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT7_OFFSET (Eid),                    "Local Sapic EID", 0},
    {ACPI_DMT_UINT24,   ACPI_MADT7_OFFSET (Reserved[0]),            "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT7_OFFSET (LapicFlags),             "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT7_FLAG_OFFSET (LapicFlags,0),      "Processor Enabled", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT7_OFFSET (Uid),                    "Processor UID", 0},
    {ACPI_DMT_STRING,   ACPI_MADT7_OFFSET (UidString[0]),           "Processor UID String", 0},
    ACPI_DMT_TERMINATOR
};

/* 8: Platform Interrupt Source */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt8[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT8_OFFSET (IntiFlags),              "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAGS0,   ACPI_MADT8_FLAG_OFFSET (IntiFlags,0),       "Polarity", 0},
    {ACPI_DMT_FLAGS2,   ACPI_MADT8_FLAG_OFFSET (IntiFlags,0),       "Trigger Mode", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT8_OFFSET (Type),                   "InterruptType", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT8_OFFSET (Id),                     "Processor ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT8_OFFSET (Eid),                    "Processor EID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT8_OFFSET (IoSapicVector),          "I/O Sapic Vector", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT8_OFFSET (GlobalIrq),              "Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT8_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT8_OFFSET (Flags),                  "CPEI Override", 0},
    ACPI_DMT_TERMINATOR
};

/* 9: Processor Local X2_APIC (ACPI 4.0) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt9[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT9_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT9_OFFSET (LocalApicId),            "Processor x2Apic ID", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT9_OFFSET (LapicFlags),             "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT9_FLAG_OFFSET (LapicFlags,0),      "Processor Enabled", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT9_OFFSET (Uid),                    "Processor UID", 0},
    ACPI_DMT_TERMINATOR
};

/* 10: Local X2_APIC NMI (ACPI 4.0) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt10[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT10_OFFSET (IntiFlags),             "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAGS0,   ACPI_MADT10_FLAG_OFFSET (IntiFlags,0),      "Polarity", 0},
    {ACPI_DMT_FLAGS2,   ACPI_MADT10_FLAG_OFFSET (IntiFlags,0),      "Trigger Mode", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT10_OFFSET (Uid),                   "Processor UID", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT10_OFFSET (Lint),                  "Interrupt Input LINT", 0},
    {ACPI_DMT_UINT24,   ACPI_MADT10_OFFSET (Reserved[0]),           "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 11: Generic Interrupt Controller (ACPI 5.0) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt11[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (CpuInterfaceNumber),    "CPU Interface Number", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (Uid),                   "Processor UID", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (Flags),                 "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Processor Enabled", 0},
    {ACPI_DMT_FLAG1,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Performance Interrupt Trigger Mode", 0},
    {ACPI_DMT_FLAG2,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Virtual GIC Interrupt Trigger Mode", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (ParkingVersion),        "Parking Protocol Version", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (PerformanceInterrupt),  "Performance Interrupt", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (ParkedAddress),         "Parked Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GicvBaseAddress),       "Virtual GIC Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GichBaseAddress),       "Hypervisor GIC Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (VgicInterrupt),         "Virtual GIC Interrupt", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GicrBaseAddress),       "Redistributor Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (ArmMpidr),              "ARM MPIDR", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT11_OFFSET (EfficiencyClass),       "Efficiency Class", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT11_OFFSET (Reserved2[0]),          "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (SpeInterrupt),          "SPE Overflow Interrupt", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (TrbeInterrupt),         "TRBE Interrupt", 0},
    ACPI_DMT_TERMINATOR
};

/* 11: Generic Interrupt Controller (ACPI 5.0) - MADT revision 6 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt11a[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (CpuInterfaceNumber),    "CPU Interface Number", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (Uid),                   "Processor UID", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (Flags),                 "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Processor Enabled", 0},
    {ACPI_DMT_FLAG1,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Performance Interrupt Trigger Mode", 0},
    {ACPI_DMT_FLAG2,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Virtual GIC Interrupt Trigger Mode", 0},
    {ACPI_DMT_FLAG3,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Online Capable", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (ParkingVersion),        "Parking Protocol Version", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (PerformanceInterrupt),  "Performance Interrupt", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (ParkedAddress),         "Parked Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GicvBaseAddress),       "Virtual GIC Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GichBaseAddress),       "Hypervisor GIC Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (VgicInterrupt),         "Virtual GIC Interrupt", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GicrBaseAddress),       "Redistributor Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (ArmMpidr),              "ARM MPIDR", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT11_OFFSET (EfficiencyClass),       "Efficiency Class", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT11_OFFSET (Reserved2[0]),          "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (SpeInterrupt),          "SPE Overflow Interrupt", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (TrbeInterrupt),         "TRBE Interrupt", 0},
    ACPI_DMT_TERMINATOR
};

/* 11: Generic Interrupt Controller (ACPI 5.0) - MADT revision 7 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt11b[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (CpuInterfaceNumber),    "CPU Interface Number", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (Uid),                   "Processor UID", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (Flags),                 "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Processor Enabled", 0},
    {ACPI_DMT_FLAG1,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Performance Interrupt Trigger Mode", 0},
    {ACPI_DMT_FLAG2,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Virtual GIC Interrupt Trigger Mode", 0},
    {ACPI_DMT_FLAG3,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "Online Capable", 0},
    {ACPI_DMT_FLAG4,    ACPI_MADT11_FLAG_OFFSET (Flags,0),          "GICR non-coherent", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (ParkingVersion),        "Parking Protocol Version", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (PerformanceInterrupt),  "Performance Interrupt", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (ParkedAddress),         "Parked Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GicvBaseAddress),       "Virtual GIC Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GichBaseAddress),       "Hypervisor GIC Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT11_OFFSET (VgicInterrupt),         "Virtual GIC Interrupt", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (GicrBaseAddress),       "Redistributor Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT11_OFFSET (ArmMpidr),              "ARM MPIDR", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT11_OFFSET (EfficiencyClass),       "Efficiency Class", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT11_OFFSET (Reserved2[0]),          "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (SpeInterrupt),          "SPE Overflow Interrupt", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT11_OFFSET (TrbeInterrupt),         "TRBE Interrupt", 0},
    ACPI_DMT_TERMINATOR
};

/* 12: Generic Interrupt Distributor (ACPI 5.0) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt12[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT12_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT12_OFFSET (GicId),                 "Local GIC Hardware ID", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT12_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT12_OFFSET (GlobalIrqBase),         "Interrupt Base", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT12_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT24,   ACPI_MADT12_OFFSET (Reserved2[0]),          "Reserved", 0},
   ACPI_DMT_TERMINATOR
};

/* 13: Generic MSI Frame (ACPI 5.1) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt13[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT13_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT13_OFFSET (MsiFrameId),            "MSI Frame ID", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT13_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT13_OFFSET (Flags),                 "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT13_FLAG_OFFSET (Flags,0),          "Select SPI", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT13_OFFSET (SpiCount),              "SPI Count", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT13_OFFSET (SpiBase),               "SPI Base", 0},
   ACPI_DMT_TERMINATOR
};

/* 14: Generic Redistributor (ACPI 5.1) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt14[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT14_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT14_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT14_OFFSET (Length),                "Length", 0},
   ACPI_DMT_TERMINATOR
};

/* 14: Generic Redistributor (ACPI 5.1) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt14a[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT14_OFFSET (Flags),                 "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT14_FLAG_OFFSET (Flags,0),          "GICR non-coherent", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT14_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT14_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT14_OFFSET (Length),                "Length", 0},
   ACPI_DMT_TERMINATOR
};

/* 15: Generic Translator (ACPI 6.0) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt15[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT15_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT15_OFFSET (TranslationId),         "Translation ID", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT15_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT15_OFFSET (Reserved2),             "Reserved", 0},
   ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt15a[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT15_OFFSET (Flags),                 "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MADT15_FLAG_OFFSET (Flags,0),          "GIC ITS non-coherent", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT15_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT15_OFFSET (TranslationId),         "Translation ID", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT15_OFFSET (BaseAddress),           "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT15_OFFSET (Reserved2),             "Reserved", 0},
   ACPI_DMT_TERMINATOR
};

/* 16: Multiprocessor wakeup structure (ACPI 6.4) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt16[] =
{
    {ACPI_DMT_UINT16,   ACPI_MADT16_OFFSET (MailboxVersion),        "Mailbox Version", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT16_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT16_OFFSET (BaseAddress),           "Mailbox Address", 0},
   ACPI_DMT_TERMINATOR
};

/* 17: core interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt17[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT17_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT17_OFFSET (ProcessorId),           "ProcessorId", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT17_OFFSET (CoreId),                "CoreId", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT17_OFFSET (Flags),                 "Flags", 0},
   ACPI_DMT_TERMINATOR
};

/* 18: Legacy I/O interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt18[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT18_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT18_OFFSET (Address),               "Address", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT18_OFFSET (Size),                  "Size", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT18_OFFSET (Cascade),               "Cascade", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT18_OFFSET (CascadeMap),            "CascadeMap", 0},
   ACPI_DMT_TERMINATOR
};

/* 19: HT interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt19[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT19_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT19_OFFSET (Address),               "Address", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT19_OFFSET (Size),                  "Size", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT19_OFFSET (Cascade),               "Cascade", 0},
   ACPI_DMT_TERMINATOR
};

/* 20: Extend I/O interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt20[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT20_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT20_OFFSET (Cascade),               "Cascade", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT20_OFFSET (Node),                  "Node", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT20_OFFSET (NodeMap),               "NodeMap", 0},
   ACPI_DMT_TERMINATOR
};

/* 21: MSI controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt21[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT21_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT21_OFFSET (MsgAddress),            "MsgAddress", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT21_OFFSET (Start),                 "Start", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT21_OFFSET (Count),                 "Count", 0},
   ACPI_DMT_TERMINATOR
};

/* 22: BIO interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt22[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT22_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT22_OFFSET (Address),               "Address", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT22_OFFSET (Size),                  "Size", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT22_OFFSET (Id),                    "Id", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT22_OFFSET (GsiBase),               "GsiBase", 0},
   ACPI_DMT_TERMINATOR
};

/* 23: LPC interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt23[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT23_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT23_OFFSET (Address),               "Address", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT23_OFFSET (Size),                  "Size", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT23_OFFSET (Cascade),               "Cascade", 0},
   ACPI_DMT_TERMINATOR
};

/* 24: RINTC interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt24[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT24_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT24_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT24_OFFSET (Flags),                 "Flags", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT24_OFFSET (HartId),                "HartId", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT24_OFFSET (Uid),                   "Uid", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT24_OFFSET (ExtIntcId),             "ExtIntcId", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT24_OFFSET (ImsicAddr),             "ImsicAddr", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT24_OFFSET (ImsicSize),             "ImsicSize", 0},
   ACPI_DMT_TERMINATOR
};

/* 25: RISC-V IMSIC interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt25[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT25_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT25_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT25_OFFSET (Flags),                 "Flags", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT25_OFFSET (NumIds),                "NumIds", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT25_OFFSET (NumGuestIds),           "NumGuestIds", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT25_OFFSET (GuestIndexBits),        "GuestIndexBits", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT25_OFFSET (HartIndexBits),         "HartIndexBits", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT25_OFFSET (GroupIndexBits),        "GroupIndexBits", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT25_OFFSET (GroupIndexShift),       "GroupIndexShift", 0},
   ACPI_DMT_TERMINATOR
};

/* 26: RISC-V APLIC interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt26[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT26_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT26_OFFSET (Id),                    "Id", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT26_OFFSET (Flags),                 "Flags", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT26_OFFSET (HwId),                  "HwId", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT26_OFFSET (NumIdcs),               "NumIdcs", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT26_OFFSET (NumSources),            "NumSources", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT26_OFFSET (GsiBase),               "GsiBase", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT26_OFFSET (BaseAddr),              "BaseAddr", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT26_OFFSET (Size),                  "Size", 0},
   ACPI_DMT_TERMINATOR
};

/* 27: RISC-V PLIC interrupt controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt27[] =
{
    {ACPI_DMT_UINT8,    ACPI_MADT27_OFFSET (Version),               "Version", 0},
    {ACPI_DMT_UINT8,    ACPI_MADT27_OFFSET (Id),                    "Id", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT27_OFFSET (HwId),                  "HwId", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT27_OFFSET (NumIrqs),               "NumIrqs", 0},
    {ACPI_DMT_UINT16,   ACPI_MADT27_OFFSET (MaxPrio),               "MaxPrio", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT27_OFFSET (Flags),                 "Flags", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT27_OFFSET (Size),                  "Size", 0},
    {ACPI_DMT_UINT64,   ACPI_MADT27_OFFSET (BaseAddr),              "BaseAddr", 0},
    {ACPI_DMT_UINT32,   ACPI_MADT27_OFFSET (GsiBase),               "GsiBase", 0},
   ACPI_DMT_TERMINATOR
};

/* 128: OEM data structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMadt128[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "OEM Data", 0},
   ACPI_DMT_TERMINATOR
};

/*******************************************************************************
 *
 * MCFG - PCI Memory Mapped Configuration table and Subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoMcfg[] =
{
    {ACPI_DMT_UINT64,   ACPI_MCFG_OFFSET (Reserved[0]),             "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoMcfg0[] =
{
    {ACPI_DMT_UINT64,   ACPI_MCFG0_OFFSET (Address),                "Base Address", 0},
    {ACPI_DMT_UINT16,   ACPI_MCFG0_OFFSET (PciSegment),             "Segment Group Number", 0},
    {ACPI_DMT_UINT8,    ACPI_MCFG0_OFFSET (StartBusNumber),         "Start Bus Number", 0},
    {ACPI_DMT_UINT8,    ACPI_MCFG0_OFFSET (EndBusNumber),           "End Bus Number", 0},
    {ACPI_DMT_UINT32,   ACPI_MCFG0_OFFSET (Reserved),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * MCHI - Management Controller Host Interface table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoMchi[] =
{
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (InterfaceType),           "Interface Type", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (Protocol),                "Protocol", 0},
    {ACPI_DMT_UINT64,   ACPI_MCHI_OFFSET (ProtocolData),            "Protocol Data", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (InterruptType),           "Interrupt Type", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (Gpe),                     "Gpe", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (PciDeviceFlag),           "Pci Device Flag", 0},
    {ACPI_DMT_UINT32,   ACPI_MCHI_OFFSET (GlobalInterrupt),         "Global Interrupt", 0},
    {ACPI_DMT_GAS,      ACPI_MCHI_OFFSET (ControlRegister),         "Control Register", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (PciSegment),              "Pci Segment", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (PciBus),                  "Pci Bus", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (PciDevice),               "Pci Device", 0},
    {ACPI_DMT_UINT8,    ACPI_MCHI_OFFSET (PciFunction),             "Pci Function", 0},
    ACPI_DMT_TERMINATOR
};

/*******************************************************************************
 *
 * MPAM - Memory System Resource Partitioning and Monitoring Tables
 * Arm's DEN0065 MPAM ACPI 2.0. December 2022.
 ******************************************************************************/

/* MPAM subtables */

/* 0: MPAM Resource Node Structure - A root MSC table.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 4: MPAM MSC node body.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam0[] =
{
    {ACPI_DMT_UINT16,   ACPI_MPAM0_OFFSET (Length),                      "Length", 0},
    {ACPI_DMT_UINT8,    ACPI_MPAM0_OFFSET (InterfaceType),               "Interface type", 0},
    {ACPI_DMT_UINT8,    ACPI_MPAM0_OFFSET (Reserved),                    "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (Identifier),                  "Identifier", 0},
    {ACPI_DMT_UINT64,   ACPI_MPAM0_OFFSET (BaseAddress),                 "Base address", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (MMIOSize),                    "MMIO size", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (OverflowInterrupt),           "Overflow interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (OverflowInterruptFlags),      "Overflow interrupt flags", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (Reserved1),                   "Reserved1", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (OverflowInterruptAffinity),   "Overflow interrupt affinity", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (ErrorInterrupt),              "Error interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (ErrorInterruptFlags),         "Error interrupt flags", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (Reserved2),                   "Reserved2", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (ErrorInterruptAffinity),      "Error interrupt affinity", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (MaxNrdyUsec),                 "MAX_NRDY_USEC", 0},
    {ACPI_DMT_NAME8,    ACPI_MPAM0_OFFSET (HardwareIdLinkedDevice),      "Hardware ID of linked device", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (InstanceIdLinkedDevice),      "Instance ID of linked device", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM0_OFFSET (NumResourceNodes),            "Number of resource nodes", 0},

    ACPI_DMT_TERMINATOR
};

/* 1: MPAM Resource (RIS) Node Structure - A subtable of MSC Nodes.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 9: Resource node.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1[] =
{
    {ACPI_DMT_UINT32,          ACPI_MPAM1_OFFSET (Identifier),              "Identifier", 0},
    {ACPI_DMT_UINT8,           ACPI_MPAM1_OFFSET (RISIndex),                "RIS Index", 0},
    {ACPI_DMT_UINT16,          ACPI_MPAM1_OFFSET (Reserved1),               "Reserved1", 0},
    {ACPI_DMT_MPAM_LOCATOR,    ACPI_MPAM1_OFFSET (LocatorType),             "Locator type", 0},
    ACPI_DMT_TERMINATOR
};

/* An RIS field part of the RIS subtable */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1Deps[] =
{
    {ACPI_DMT_UINT32, 0, "Number of functional dependencies", 0},
    ACPI_DMT_TERMINATOR
};

/* 1A: MPAM Processor cache locator descriptor. A subtable of RIS.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 13.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1A[] =
{
    {ACPI_DMT_UINT64,   ACPI_MPAM1A_OFFSET (CacheReference),                "Cache reference", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM1A_OFFSET (Reserved),                      "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 1B: MPAM Memory locator descriptor. A subtable of RIS.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 14.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1B[] =
{
    {ACPI_DMT_UINT64,   ACPI_MPAM1B_OFFSET (ProximityDomain),               "Proximity domain", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM1B_OFFSET (Reserved),                      "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 1C: MPAM SMMU locator descriptor. A subtable of RIS.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 15.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1C[] =
{
    {ACPI_DMT_UINT64,   ACPI_MPAM1C_OFFSET (SmmuInterface),                 "SMMU Interface", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM1C_OFFSET (Reserved),                      "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 1D: MPAM Memory-side cache locator descriptor. A subtable of RIS.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 16.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1D[] =
{
    {ACPI_DMT_UINT56,   ACPI_MPAM1D_OFFSET (Reserved),                      "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_MPAM1D_OFFSET (Level),                         "Level", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM1D_OFFSET (Reference),                     "Reference", 0},
    ACPI_DMT_TERMINATOR
};

/* 1E: MPAM ACPI device locator descriptor. A subtable of RIS.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 17.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1E[] =
{
    {ACPI_DMT_UINT64,   ACPI_MPAM1E_OFFSET (AcpiHwId),                      "ACPI Hardware ID", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM1E_OFFSET (AcpiUniqueId),                  "ACPI Unique ID", 0},
    ACPI_DMT_TERMINATOR
};

/* 1F: MPAM Interconnect locator descriptor. A subtable of RIS.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 18.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1F[] =
{
    {ACPI_DMT_UINT64,   ACPI_MPAM1F_OFFSET (InterConnectDescTblOff),        "Interconnect descriptor table offset", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM1F_OFFSET (Reserved),                      "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 1G: MPAM Locator structure.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 12.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam1G[] =
{
    {ACPI_DMT_UINT64,   ACPI_MPAM1G_OFFSET (Descriptor1),                   "Descriptor1", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM1G_OFFSET (Descriptor2),                   "Descriptor2", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: MPAM Functional dependency descriptor.
 * Arm's DEN0065 MPAM ACPI 2.0. Table 10.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoMpam2[] =
{
    {ACPI_DMT_UINT32,   ACPI_MPAM2_OFFSET (Producer),                       "Producer", 0},
    {ACPI_DMT_UINT32,   ACPI_MPAM2_OFFSET (Reserved),                       "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * MPST - Memory Power State Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoMpst[] =
{
    {ACPI_DMT_UINT8,    ACPI_MPST_OFFSET (ChannelId),               "Channel ID", 0},
    {ACPI_DMT_UINT24,   ACPI_MPST_OFFSET (Reserved1[0]),            "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_MPST_OFFSET (PowerNodeCount),          "Power Node Count", 0},
    {ACPI_DMT_UINT16,   ACPI_MPST_OFFSET (Reserved2),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* MPST subtables */

/* 0: Memory Power Node Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMpst0[] =
{
    {ACPI_DMT_UINT8,    ACPI_MPST0_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MPST0_FLAG_OFFSET (Flags,0),           "Node Enabled", 0},
    {ACPI_DMT_FLAG1,    ACPI_MPST0_FLAG_OFFSET (Flags,0),           "Power Managed", 0},
    {ACPI_DMT_FLAG2,    ACPI_MPST0_FLAG_OFFSET (Flags,0),           "Hot Plug Capable", 0},

    {ACPI_DMT_UINT8,    ACPI_MPST0_OFFSET (Reserved1),              "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_MPST0_OFFSET (NodeId),                 "Node ID", 0},
    {ACPI_DMT_UINT32,   ACPI_MPST0_OFFSET (Length),                 "Length", 0},
    {ACPI_DMT_UINT64,   ACPI_MPST0_OFFSET (RangeAddress),           "Range Address", 0},
    {ACPI_DMT_UINT64,   ACPI_MPST0_OFFSET (RangeLength),            "Range Length", 0},
    {ACPI_DMT_UINT32,   ACPI_MPST0_OFFSET (NumPowerStates),         "Num Power States", 0},
    {ACPI_DMT_UINT32,   ACPI_MPST0_OFFSET (NumPhysicalComponents),  "Num Physical Components", 0},
    ACPI_DMT_TERMINATOR
};

/* 0A: Sub-subtable - Memory Power State Structure (follows Memory Power Node above) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMpst0A[] =
{
    {ACPI_DMT_UINT8,    ACPI_MPST0A_OFFSET (PowerState),            "Power State", 0},
    {ACPI_DMT_UINT8,    ACPI_MPST0A_OFFSET (InfoIndex),             "InfoIndex", 0},
    ACPI_DMT_TERMINATOR
};

/* 0B: Sub-subtable - Physical Component ID Structure (follows Memory Power State(s) above) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMpst0B[] =
{
    {ACPI_DMT_UINT16,   ACPI_MPST0B_OFFSET (ComponentId),           "Component Id", 0},
    ACPI_DMT_TERMINATOR
};

/* 01: Power Characteristics Count (follows all Power Node(s) above) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMpst1[] =
{
    {ACPI_DMT_UINT16,   ACPI_MPST1_OFFSET (CharacteristicsCount),   "Characteristics Count", 0},
    {ACPI_DMT_UINT16,   ACPI_MPST1_OFFSET (Reserved),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 02: Memory Power State Characteristics Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMpst2[] =
{
    {ACPI_DMT_UINT8,    ACPI_MPST2_OFFSET (StructureId),            "Structure ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MPST2_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_MPST2_FLAG_OFFSET (Flags,0),           "Memory Preserved", 0},
    {ACPI_DMT_FLAG1,    ACPI_MPST2_FLAG_OFFSET (Flags,0),           "Auto Entry", 0},
    {ACPI_DMT_FLAG2,    ACPI_MPST2_FLAG_OFFSET (Flags,0),           "Auto Exit", 0},

    {ACPI_DMT_UINT16,   ACPI_MPST2_OFFSET (Reserved1),              "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_MPST2_OFFSET (AveragePower),           "Average Power", 0},
    {ACPI_DMT_UINT32,   ACPI_MPST2_OFFSET (PowerSaving),            "Power Saving", 0},
    {ACPI_DMT_UINT64,   ACPI_MPST2_OFFSET (ExitLatency),            "Exit Latency", 0},
    {ACPI_DMT_UINT64,   ACPI_MPST2_OFFSET (Reserved2),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * MRRM - Memory Range and Region Mapping Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoMrrm[] =
{
    {ACPI_DMT_UINT8,    ACPI_MRRM_OFFSET (MaxMemRegion),            "Max Memory Regions", 0},
    {ACPI_DMT_UINT8,    ACPI_MRRM_OFFSET (Flags),                   "Region Assignment Type", 0},
    {ACPI_DMT_BUF26,    ACPI_MRRM_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* MRRM Subtable */

/* 0: Memory Range entry */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMrrm0[] =
{
    {ACPI_DMT_UINT16,   ACPI_MRRM0_OFFSET (Header.Type),            "Memory Range", 0},
    {ACPI_DMT_UINT16,   ACPI_MRRM0_OFFSET (Header.Length),          "Length", DT_LENGTH},
    {ACPI_DMT_UINT32,   ACPI_MRRM0_OFFSET (Reserved0),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_MRRM0_OFFSET (AddrBase),               "System Address Base", 0},
    {ACPI_DMT_UINT64,   ACPI_MRRM0_OFFSET (AddrLen),                "System Address Length", 0},
    {ACPI_DMT_UINT16,   ACPI_MRRM0_OFFSET (RegionIdFlags),          "Region Valid Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_MRRM0_OFFSET (LocalRegionId),          "Static Local Region ID", 0},
    {ACPI_DMT_UINT8,    ACPI_MRRM0_OFFSET (RemoteRegionId),         "Static Remote Region ID", 0},
    {ACPI_DMT_UINT32,   ACPI_MRRM0_OFFSET (Reserved1),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * MSCT - Maximum System Characteristics Table (ACPI 4.0)
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoMsct[] =
{
    {ACPI_DMT_UINT32,   ACPI_MSCT_OFFSET (ProximityOffset),         "Proximity Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_MSCT_OFFSET (MaxProximityDomains),     "Max Proximity Domains", 0},
    {ACPI_DMT_UINT32,   ACPI_MSCT_OFFSET (MaxClockDomains),         "Max Clock Domains", 0},
    {ACPI_DMT_UINT64,   ACPI_MSCT_OFFSET (MaxAddress),              "Max Physical Address", 0},
    ACPI_DMT_TERMINATOR
};

/* Subtable - Maximum Proximity Domain Information. Version 1 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoMsct0[] =
{
    {ACPI_DMT_UINT8,    ACPI_MSCT0_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT8,    ACPI_MSCT0_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT32,   ACPI_MSCT0_OFFSET (RangeStart),             "Domain Range Start", 0},
    {ACPI_DMT_UINT32,   ACPI_MSCT0_OFFSET (RangeEnd),               "Domain Range End", 0},
    {ACPI_DMT_UINT32,   ACPI_MSCT0_OFFSET (ProcessorCapacity),      "Processor Capacity", 0},
    {ACPI_DMT_UINT64,   ACPI_MSCT0_OFFSET (MemoryCapacity),         "Memory Capacity", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * NFIT - NVDIMM Firmware Interface Table and Subtables - (ACPI 6.0)
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit[] =
{
    {ACPI_DMT_UINT32,   ACPI_NFIT_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfitHdr[] =
{
    {ACPI_DMT_NFIT,     ACPI_NFITH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT16,   ACPI_NFITH_OFFSET (Length),                 "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* 0: System Physical Address Range Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit0[] =
{
    {ACPI_DMT_UINT16,   ACPI_NFIT0_OFFSET (RangeIndex),             "Range Index", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT0_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_NFIT0_FLAG_OFFSET (Flags,0),           "Add/Online Operation Only", 0},
    {ACPI_DMT_FLAG1,    ACPI_NFIT0_FLAG_OFFSET (Flags,0),           "Proximity Domain Valid", 0},
    {ACPI_DMT_FLAG2,    ACPI_NFIT0_FLAG_OFFSET (Flags,0),           "Location Cookie Valid", 0},
    {ACPI_DMT_UINT32,   ACPI_NFIT0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_NFIT0_OFFSET (ProximityDomain),        "Proximity Domain", 0},
    {ACPI_DMT_UUID,     ACPI_NFIT0_OFFSET (RangeGuid[0]),           "Region Type GUID", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT0_OFFSET (Address),                "Address Range Base", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT0_OFFSET (Length),                 "Address Range Length", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT0_OFFSET (MemoryMapping),          "Memory Map Attribute", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT0_OFFSET (LocationCookie),         "Location Cookie", 0},      /* ACPI 6.4 */
    ACPI_DMT_TERMINATOR
};

/* 1: Memory Device to System Address Range Map Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit1[] =
{
    {ACPI_DMT_UINT32,   ACPI_NFIT1_OFFSET (DeviceHandle),           "Device Handle", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (PhysicalId),             "Physical Id", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (RegionId),               "Region Id", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (RangeIndex),             "Range Index", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (RegionIndex),            "Control Region Index", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT1_OFFSET (RegionSize),             "Region Size", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT1_OFFSET (RegionOffset),           "Region Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT1_OFFSET (Address),                "Address Region Base", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (InterleaveIndex),        "Interleave Index", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (InterleaveWays),         "Interleave Ways", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (Flags),                  "Flags", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_NFIT1_FLAG_OFFSET (Flags,0),           "Save to device failed", 0},
    {ACPI_DMT_FLAG1,    ACPI_NFIT1_FLAG_OFFSET (Flags,0),           "Restore from device failed", 0},
    {ACPI_DMT_FLAG2,    ACPI_NFIT1_FLAG_OFFSET (Flags,0),           "Platform flush failed", 0},
    {ACPI_DMT_FLAG3,    ACPI_NFIT1_FLAG_OFFSET (Flags,0),           "Device not armed", 0},
    {ACPI_DMT_FLAG4,    ACPI_NFIT1_FLAG_OFFSET (Flags,0),           "Health events observed", 0},
    {ACPI_DMT_FLAG5,    ACPI_NFIT1_FLAG_OFFSET (Flags,0),           "Health events enabled", 0},
    {ACPI_DMT_FLAG6,    ACPI_NFIT1_FLAG_OFFSET (Flags,0),           "Mapping failed", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT1_OFFSET (Reserved),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: Interleave Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit2[] =
{
    {ACPI_DMT_UINT16,   ACPI_NFIT2_OFFSET (InterleaveIndex),        "Interleave Index", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT2_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_NFIT2_OFFSET (LineCount),              "Line Count", 0},
    {ACPI_DMT_UINT32,   ACPI_NFIT2_OFFSET (LineSize),               "Line Size", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit2a[] =
{
    {ACPI_DMT_UINT32,   0,                                          "Line Offset", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 3: SMBIOS Management Information Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit3[] =
{
    {ACPI_DMT_UINT32,   ACPI_NFIT3_OFFSET (Reserved),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit3a[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "SMBIOS Table Entries", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 4: NVDIMM Control Region Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit4[] =
{
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (RegionIndex),            "Region Index", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (VendorId),               "Vendor Id", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (DeviceId),               "Device Id", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (RevisionId),             "Revision Id", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (SubsystemVendorId),      "Subsystem Vendor Id", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (SubsystemDeviceId),      "Subsystem Device Id", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (SubsystemRevisionId),    "Subsystem Revision Id", 0},
    {ACPI_DMT_UINT8,    ACPI_NFIT4_OFFSET (ValidFields),            "Valid Fields", 0},
    {ACPI_DMT_UINT8,    ACPI_NFIT4_OFFSET (ManufacturingLocation),  "Manufacturing Location", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (ManufacturingDate),      "Manufacturing Date", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (Reserved[0]),            "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_NFIT4_OFFSET (SerialNumber),           "Serial Number", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (Code),                   "Code", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (Windows),                "Window Count", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT4_OFFSET (WindowSize),             "Window Size", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT4_OFFSET (CommandOffset),          "Command Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT4_OFFSET (CommandSize),            "Command Size", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT4_OFFSET (StatusOffset),           "Status Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT4_OFFSET (StatusSize),             "Status Size", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT4_OFFSET (Flags),                  "Flags", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_NFIT4_FLAG_OFFSET (Flags,0),           "Windows buffered", 0},
    {ACPI_DMT_UINT48,   ACPI_NFIT4_OFFSET (Reserved1[0]),           "Reserved1", 0},
    ACPI_DMT_TERMINATOR
};

/* 5: NVDIMM Block Data Window Region Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit5[] =
{
    {ACPI_DMT_UINT16,   ACPI_NFIT5_OFFSET (RegionIndex),            "Region Index", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT5_OFFSET (Windows),                "Window Count", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT5_OFFSET (Offset),                 "Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT5_OFFSET (Size),                   "Size", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT5_OFFSET (Capacity),               "Capacity", 0},
    {ACPI_DMT_UINT64,   ACPI_NFIT5_OFFSET (StartAddress),           "Start Address", 0},
    ACPI_DMT_TERMINATOR
};

/* 6: Flush Hint Address Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit6[] =
{
    {ACPI_DMT_UINT32,   ACPI_NFIT6_OFFSET (DeviceHandle),           "Device Handle", 0},
    {ACPI_DMT_UINT16,   ACPI_NFIT6_OFFSET (HintCount),              "Hint Count", 0},
    {ACPI_DMT_UINT48,   ACPI_NFIT6_OFFSET (Reserved[0]),            "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit6a[] =
{
    {ACPI_DMT_UINT64,   0,                                          "Hint Address", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoNfit7[] =
{
    {ACPI_DMT_UINT8,    ACPI_NFIT7_OFFSET (HighestCapability),      "Highest Capability", 0},
    {ACPI_DMT_UINT24,   ACPI_NFIT7_OFFSET (Reserved[0]),            "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_NFIT7_OFFSET (Capabilities),           "Capabilities (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_NFIT7_FLAG_OFFSET (Capabilities,0),    "Cache Flush to NVDIMM", 0},
    {ACPI_DMT_FLAG1,    ACPI_NFIT7_FLAG_OFFSET (Capabilities,0),    "Memory Flush to NVDIMM", 0},
    {ACPI_DMT_FLAG2,    ACPI_NFIT7_FLAG_OFFSET (Capabilities,0),    "Memory Mirroring", 0},
    {ACPI_DMT_UINT32,   ACPI_NFIT7_OFFSET (Reserved2),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * PCCT - Platform Communications Channel Table (ACPI 5.0)
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcct[] =
{
    {ACPI_DMT_UINT32,   ACPI_PCCT_OFFSET (Flags),                   "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_PCCT_FLAG_OFFSET (Flags,0),            "Platform", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* PCCT subtables */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcctHdr[] =
{
    {ACPI_DMT_PCCT,     ACPI_PCCT0_OFFSET (Header.Type),            "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT0_OFFSET (Header.Length),          "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* 0: Generic Communications Subspace */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcct0[] =
{
    {ACPI_DMT_UINT48,   ACPI_PCCT0_OFFSET (Reserved[0]),            "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT0_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT0_OFFSET (Length),                 "Address Length", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT0_OFFSET (DoorbellRegister),       "Doorbell Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT0_OFFSET (PreserveMask),           "Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT0_OFFSET (WriteMask),              "Write Mask", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT0_OFFSET (Latency),                "Command Latency", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT0_OFFSET (MaxAccessRate),          "Maximum Access Rate", 0},
    {ACPI_DMT_UINT16,   ACPI_PCCT0_OFFSET (MinTurnaroundTime),      "Minimum Turnaround Time", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: HW-reduced Communications Subspace (ACPI 5.1) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcct1[] =
{
    {ACPI_DMT_UINT32,   ACPI_PCCT1_OFFSET (PlatformInterrupt),      "Platform Interrupt", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT1_OFFSET (Flags),                  "Flags (Decoded Below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_PCCT1_FLAG_OFFSET (Flags,0),           "Polarity", 0},
    {ACPI_DMT_FLAG1,    ACPI_PCCT1_FLAG_OFFSET (Flags,0),           "Mode", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT1_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT1_OFFSET (Length),                 "Address Length", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT1_OFFSET (DoorbellRegister),       "Doorbell Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT1_OFFSET (PreserveMask),           "Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT1_OFFSET (WriteMask),              "Write Mask", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT1_OFFSET (Latency),                "Command Latency", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT1_OFFSET (MaxAccessRate),          "Maximum Access Rate", 0},
    {ACPI_DMT_UINT16,   ACPI_PCCT1_OFFSET (MinTurnaroundTime),      "Minimum Turnaround Time", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: HW-reduced Communications Subspace Type 2 (ACPI 6.1) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcct2[] =
{
    {ACPI_DMT_UINT32,   ACPI_PCCT2_OFFSET (PlatformInterrupt),      "Platform Interrupt", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT2_OFFSET (Flags),                  "Flags (Decoded Below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_PCCT2_FLAG_OFFSET (Flags,0),           "Polarity", 0},
    {ACPI_DMT_FLAG1,    ACPI_PCCT2_FLAG_OFFSET (Flags,0),           "Mode", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT2_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT2_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT2_OFFSET (Length),                 "Address Length", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT2_OFFSET (DoorbellRegister),       "Doorbell Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT2_OFFSET (PreserveMask),           "Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT2_OFFSET (WriteMask),              "Write Mask", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT2_OFFSET (Latency),                "Command Latency", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT2_OFFSET (MaxAccessRate),          "Maximum Access Rate", 0},
    {ACPI_DMT_UINT16,   ACPI_PCCT2_OFFSET (MinTurnaroundTime),      "Minimum Turnaround Time", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT2_OFFSET (PlatformAckRegister),    "Platform ACK Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT2_OFFSET (AckPreserveMask),        "ACK Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT2_OFFSET (AckWriteMask),           "ACK Write Mask", 0},
    ACPI_DMT_TERMINATOR
};

/* 3: Extended PCC Master Subspace Type 3 (ACPI 6.2) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcct3[] =
{
    {ACPI_DMT_UINT32,   ACPI_PCCT3_OFFSET (PlatformInterrupt),      "Platform Interrupt", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT3_OFFSET (Flags),                  "Flags (Decoded Below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_PCCT3_FLAG_OFFSET (Flags,0),           "Polarity", 0},
    {ACPI_DMT_FLAG1,    ACPI_PCCT3_FLAG_OFFSET (Flags,0),           "Mode", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT3_OFFSET (Reserved1),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT3_OFFSET (Length),                 "Address Length", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT3_OFFSET (DoorbellRegister),       "Doorbell Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (PreserveMask),           "Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (WriteMask),              "Write Mask", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT3_OFFSET (Latency),                "Command Latency", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT3_OFFSET (MaxAccessRate),          "Maximum Access Rate", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT3_OFFSET (MinTurnaroundTime),      "Minimum Turnaround Time", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT3_OFFSET (PlatformAckRegister),    "Platform ACK Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (AckPreserveMask),        "ACK Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (AckSetMask),             "ACK Set Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (Reserved2),              "Reserved", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT3_OFFSET (CmdCompleteRegister),    "Command Complete Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (CmdCompleteMask),        "Command Complete Check Mask", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT3_OFFSET (CmdUpdateRegister),      "Command Update Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (CmdUpdatePreserveMask),  "Command Update Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (CmdUpdateSetMask),       "Command Update Set Mask", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT3_OFFSET (ErrorStatusRegister),    "Error Status Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT3_OFFSET (ErrorStatusMask),        "Error Status Mask", 0},
    ACPI_DMT_TERMINATOR
};

/* 4: Extended PCC Slave Subspace Type 4 (ACPI 6.2) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcct4[] =
{
    {ACPI_DMT_UINT32,   ACPI_PCCT4_OFFSET (PlatformInterrupt),      "Platform Interrupt", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT4_OFFSET (Flags),                  "Flags (Decoded Below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_PCCT4_FLAG_OFFSET (Flags,0),           "Polarity", 0},
    {ACPI_DMT_FLAG1,    ACPI_PCCT4_FLAG_OFFSET (Flags,0),           "Mode", 0},
    {ACPI_DMT_UINT8,    ACPI_PCCT4_OFFSET (Reserved1),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT4_OFFSET (Length),                 "Address Length", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT4_OFFSET (DoorbellRegister),       "Doorbell Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (PreserveMask),           "Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (WriteMask),              "Write Mask", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT4_OFFSET (Latency),                "Command Latency", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT4_OFFSET (MaxAccessRate),          "Maximum Access Rate", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT4_OFFSET (MinTurnaroundTime),      "Minimum Turnaround Time", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT4_OFFSET (PlatformAckRegister),    "Platform ACK Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (AckPreserveMask),        "ACK Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (AckSetMask),             "ACK Set Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (Reserved2),              "Reserved", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT4_OFFSET (CmdCompleteRegister),    "Command Complete Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (CmdCompleteMask),        "Command Complete Check Mask", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT4_OFFSET (CmdUpdateRegister),      "Command Update Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (CmdUpdatePreserveMask),  "Command Update Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (CmdUpdateSetMask),       "Command Update Set Mask", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT4_OFFSET (ErrorStatusRegister),    "Error Status Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT4_OFFSET (ErrorStatusMask),        "Error Status Mask", 0},
    ACPI_DMT_TERMINATOR
};

/* 5: HW Registers based Communications Subspace */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPcct5[] =
{
    {ACPI_DMT_UINT16,   ACPI_PCCT5_OFFSET (Version),                "Version", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT5_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT5_OFFSET (Length),                 "Length", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT5_OFFSET (DoorbellRegister),       "Doorbell Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT5_OFFSET (DoorbellPreserve),       "Preserve Mask", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT5_OFFSET (DoorbellWrite),          "Write Mask", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT5_OFFSET (CmdCompleteRegister),    "Command Complete Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT5_OFFSET (CmdCompleteMask),        "Command Complete Check Mask", 0},
    {ACPI_DMT_GAS,      ACPI_PCCT5_OFFSET (ErrorStatusRegister),    "Error Status Register", 0},
    {ACPI_DMT_UINT64,   ACPI_PCCT5_OFFSET (ErrorStatusMask),        "Error Status Mask", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT5_OFFSET (NominalLatency),         "Nominal Latency", 0},
    {ACPI_DMT_UINT32,   ACPI_PCCT5_OFFSET (MinTurnaroundTime),      "Minimum Turnaround Time", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * PDTT - Platform Debug Trigger Table (ACPI 6.2)
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoPdtt[] =
{
    {ACPI_DMT_UINT8,    ACPI_PDTT_OFFSET (TriggerCount),            "Trigger Count", 0},
    {ACPI_DMT_UINT24,   ACPI_PDTT_OFFSET (Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_PDTT_OFFSET (ArrayOffset),             "Array Offset", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoPdtt0[] =
{
    {ACPI_DMT_UINT8,    ACPI_PDTT0_OFFSET (SubchannelId),           "Subchannel Id", 0},
    {ACPI_DMT_UINT8,    ACPI_PDTT0_OFFSET (Flags),                  "Flags (Decoded Below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_PDTT0_FLAG_OFFSET (Flags,0),           "Runtime Trigger", 0},
    {ACPI_DMT_FLAG1,    ACPI_PDTT0_FLAG_OFFSET (Flags,0),           "Wait for Completion", 0},
    {ACPI_DMT_FLAG2,    ACPI_PDTT0_FLAG_OFFSET (Flags,0),           "Trigger Order", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * PHAT - Platform Health Assessment Table (ACPI 6.4)
 *
 ******************************************************************************/

/* Common subtable header */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPhatHdr[] =
{
    {ACPI_DMT_PHAT,     ACPI_PHATH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT16,   ACPI_PHATH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_PHATH_OFFSET (Revision),               "Revision", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: Firmware version table */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPhat0[] =
{
    {ACPI_DMT_UINT24,   ACPI_PHAT0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_PHAT0_OFFSET (ElementCount),           "Element Count", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoPhat0a[] =
{
    {ACPI_DMT_UUID,     ACPI_PHAT0A_OFFSET (Guid),                  "GUID", 0},
    {ACPI_DMT_UINT64,   ACPI_PHAT0A_OFFSET (VersionValue),          "Version Value", 0},
    {ACPI_DMT_UINT32,   ACPI_PHAT0A_OFFSET (ProducerId),            "Producer ID", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: Firmware Health Data Record */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPhat1[] =
{
    {ACPI_DMT_UINT16,   ACPI_PHAT1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_PHAT1_OFFSET (Health),                 "Health", 0},
    {ACPI_DMT_UUID,     ACPI_PHAT1_OFFSET (DeviceGuid),             "Device GUID", 0},
    {ACPI_DMT_UINT32,   ACPI_PHAT1_OFFSET (DeviceSpecificOffset),   "Device-Specific Offset", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoPhat1a[] =
{
    {ACPI_DMT_UNICODE, 0,                                           "Device Path", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoPhat1b[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "Device-Specific Data", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * PMTT - Platform Memory Topology Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoPmtt[] =
{
    {ACPI_DMT_UINT32,   ACPI_PMTT_OFFSET (MemoryDeviceCount),       "Memory Device Count", 0},
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header (one per Subtable) */

#define ACPI_DM_PMTT_HEADER \
    {ACPI_DMT_PMTT,     ACPI_PMTTH_OFFSET (Type),                   "Subtable Type", 0}, \
    {ACPI_DMT_UINT8,    ACPI_PMTTH_OFFSET (Reserved1),              "Reserved", 0}, \
    {ACPI_DMT_UINT16,   ACPI_PMTTH_OFFSET (Length),                 "Length", DT_LENGTH}, \
    {ACPI_DMT_UINT16,   ACPI_PMTTH_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG}, \
    {ACPI_DMT_FLAG0,    ACPI_PMTTH_FLAG_OFFSET (Flags,0),           "Top-level Device", 0}, \
    {ACPI_DMT_FLAG1,    ACPI_PMTTH_FLAG_OFFSET (Flags,0),           "Physical Element", 0}, \
    {ACPI_DMT_FLAGS2,   ACPI_PMTTH_FLAG_OFFSET (Flags,0),           "Memory Type", 0}, \
    {ACPI_DMT_UINT16,   ACPI_PMTTH_OFFSET (Reserved2),              "Reserved", 0}, \
    {ACPI_DMT_UINT32,   ACPI_PMTTH_OFFSET (MemoryDeviceCount),      "Memory Device Count", 0}

/* PMTT Subtables */

/* 0: Socket */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPmtt0[] =
{
    ACPI_DM_PMTT_HEADER,
    {ACPI_DMT_UINT16,   ACPI_PMTT0_OFFSET (SocketId),               "Socket ID", 0},
    {ACPI_DMT_UINT16,   ACPI_PMTT0_OFFSET (Reserved),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: Memory Controller */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPmtt1[] =
{
    ACPI_DM_PMTT_HEADER,
    {ACPI_DMT_UINT16,   ACPI_PMTT1_OFFSET (ControllerId),           "Controller ID", 0},
    {ACPI_DMT_UINT16,   ACPI_PMTT1_OFFSET (Reserved),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: Physical Component */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPmtt2[] =
{
    ACPI_DM_PMTT_HEADER,
    {ACPI_DMT_UINT32,   ACPI_PMTT2_OFFSET (BiosHandle),             "Bios Handle", 0},
    ACPI_DMT_TERMINATOR
};

/* 0xFF: Vendor Specific */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPmttVendor[] =
{
    ACPI_DM_PMTT_HEADER,
    {ACPI_DMT_UUID,         ACPI_PMTT_VENDOR_OFFSET (TypeUuid),     "Type Uuid", 0},
    {ACPI_DMT_PMTT_VENDOR,  ACPI_PMTT_VENDOR_OFFSET (Specific),     "Vendor Data", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * PPTT - Processor Properties Topology Table (ACPI 6.2)
 *
 ******************************************************************************/

/* Main table consists of only the standard ACPI header - subtables follow */

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPpttHdr[] =
{
    {ACPI_DMT_PPTT,     ACPI_PPTTH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_PPTTH_OFFSET (Length),                 "Length", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: Processor hierarchy node */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPptt0[] =
{
    {ACPI_DMT_UINT16,   ACPI_PPTT0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT0_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_PPTT0_FLAG_OFFSET (Flags,0),           "Physical package", 0},
    {ACPI_DMT_FLAG1,    ACPI_PPTT0_FLAG_OFFSET (Flags,0),           "ACPI Processor ID valid", 0},
    {ACPI_DMT_FLAG2,    ACPI_PPTT0_FLAG_OFFSET (Flags,0),           "Processor is a thread", 0},
    {ACPI_DMT_FLAG3,    ACPI_PPTT0_FLAG_OFFSET (Flags,0),           "Node is a leaf", 0},
    {ACPI_DMT_FLAG4,    ACPI_PPTT0_FLAG_OFFSET (Flags,0),           "Identical Implementation", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT0_OFFSET (Parent),                 "Parent", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT0_OFFSET (AcpiProcessorId),        "ACPI Processor ID", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT0_OFFSET (NumberOfPrivResources),  "Private Resource Number", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoPptt0a[] =
{
    {ACPI_DMT_UINT32,   0,                                          "Private Resource", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 1: Cache type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPptt1[] =
{
    {ACPI_DMT_UINT16,   ACPI_PPTT1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT1_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Size valid", 0},
    {ACPI_DMT_FLAG1,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Number of Sets valid", 0},
    {ACPI_DMT_FLAG2,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Associativity valid", 0},
    {ACPI_DMT_FLAG3,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Allocation Type valid", 0},
    {ACPI_DMT_FLAG4,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Cache Type valid", 0},
    {ACPI_DMT_FLAG5,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Write Policy valid", 0},
    {ACPI_DMT_FLAG6,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Line Size valid", 0},
    {ACPI_DMT_FLAG7,    ACPI_PPTT1_FLAG_OFFSET (Flags,0),           "Cache ID valid", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT1_OFFSET (NextLevelOfCache),       "Next Level of Cache", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT1_OFFSET (Size),                   "Size", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT1_OFFSET (NumberOfSets),           "Number of Sets", 0},
    {ACPI_DMT_UINT8,    ACPI_PPTT1_OFFSET (Associativity),          "Associativity", 0},
    {ACPI_DMT_UINT8,    ACPI_PPTT1_OFFSET (Attributes),             "Attributes", 0},
    {ACPI_DMT_FLAGS0,   ACPI_PPTT1_OFFSET (Attributes),             "Allocation Type", 0},
    {ACPI_DMT_FLAGS2,   ACPI_PPTT1_OFFSET (Attributes),             "Cache Type", 0},
    {ACPI_DMT_FLAG4,    ACPI_PPTT1_OFFSET (Attributes),             "Write Policy", 0},
    {ACPI_DMT_UINT16,   ACPI_PPTT1_OFFSET (LineSize),               "Line Size", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: cache type v1 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPptt1a[] =
{
    {ACPI_DMT_UINT32,   ACPI_PPTT1A_OFFSET (CacheId),               "Cache ID", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: ID */

ACPI_DMTABLE_INFO           AcpiDmTableInfoPptt2[] =
{
    {ACPI_DMT_UINT16,   ACPI_PPTT2_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_PPTT2_OFFSET (VendorId),               "Vendor ID", 0},
    {ACPI_DMT_UINT64,   ACPI_PPTT2_OFFSET (Level1Id),               "Level1 ID", 0},
    {ACPI_DMT_UINT64,   ACPI_PPTT2_OFFSET (Level2Id),               "Level2 ID", 0},
    {ACPI_DMT_UINT16,   ACPI_PPTT2_OFFSET (MajorRev),               "Major revision", 0},
    {ACPI_DMT_UINT16,   ACPI_PPTT2_OFFSET (MinorRev),               "Minor revision", 0},
    {ACPI_DMT_UINT16,   ACPI_PPTT2_OFFSET (SpinRev),                "Spin revision", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * PRMT - Platform Runtime Mechanism Table
 *        Version 1
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoPrmtHdr[] =
{
    {ACPI_DMT_UUID,     ACPI_PRMTH_OFFSET (PlatformGuid[0]),       "Platform GUID", 0},
    {ACPI_DMT_UINT32,   ACPI_PRMTH_OFFSET (ModuleInfoOffset),      "Module info offset", 0},
    {ACPI_DMT_UINT32,   ACPI_PRMTH_OFFSET (ModuleInfoCount),       "Module info count", 0},
    ACPI_DMT_NEW_LINE,
    ACPI_DMT_TERMINATOR

};

ACPI_DMTABLE_INFO           AcpiDmTableInfoPrmtModule[] =
{
    {ACPI_DMT_UINT16,   ACPI_PRMT0_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT16,   ACPI_PRMT0_OFFSET (Length),                 "Length", 0},
    {ACPI_DMT_UUID,     ACPI_PRMT0_OFFSET (ModuleGuid[0]),          "Module GUID", 0},
    {ACPI_DMT_UINT16,   ACPI_PRMT0_OFFSET (MajorRev),               "Major Revision", 0},
    {ACPI_DMT_UINT16,   ACPI_PRMT0_OFFSET (MinorRev),               "Minor Revision", 0},
    {ACPI_DMT_UINT16,   ACPI_PRMT0_OFFSET (HandlerInfoCount),       "Handler Info Count", 0},
    {ACPI_DMT_UINT32,   ACPI_PRMT0_OFFSET (HandlerInfoOffset),      "Handler Info Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_PRMT0_OFFSET (MmioListPointer),        "Mmio List pointer", 0},
    ACPI_DMT_NEW_LINE,
    ACPI_DMT_TERMINATOR

};

ACPI_DMTABLE_INFO           AcpiDmTableInfoPrmtHandler[] =
{
    {ACPI_DMT_UINT16,   ACPI_PRMT1_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT16,   ACPI_PRMT1_OFFSET (Length),                 "Length", 0},
    {ACPI_DMT_UUID,     ACPI_PRMT1_OFFSET (HandlerGuid[0]),         "Handler GUID", 0},
    {ACPI_DMT_UINT64,   ACPI_PRMT1_OFFSET (HandlerAddress),         "Handler address", 0},
    {ACPI_DMT_UINT64,   ACPI_PRMT1_OFFSET (StaticDataBufferAddress),"Static Data Address", 0},
    {ACPI_DMT_UINT64,   ACPI_PRMT1_OFFSET (AcpiParamBufferAddress), "ACPI Parameter Address", 0},
    ACPI_DMT_NEW_LINE,
    ACPI_DMT_TERMINATOR

};


/*******************************************************************************
 *
 * RASF -  RAS Feature table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoRasf[] =
{
    {ACPI_DMT_BUF12,    ACPI_RASF_OFFSET (ChannelId[0]),            "Channel ID", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RAS2 -  RAS2 Feature table (ACPI 6.5)
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoRas2[] =
{
    {ACPI_DMT_UINT16,    ACPI_RAS2_OFFSET (Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT16,    ACPI_RAS2_OFFSET (NumPccDescs),             "Number of PCC Descriptors", 0},
    ACPI_DMT_TERMINATOR
};

/* RAS2 PCC Descriptor */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRas2PccDesc[] =
{
    {ACPI_DMT_UINT8,    ACPI_RAS2_PCC_DESC_OFFSET (ChannelId),              "Channel ID", 0},
    {ACPI_DMT_UINT16,   ACPI_RAS2_PCC_DESC_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_RAS2_PCC_DESC_OFFSET (FeatureType),            "Feature Type", 0},
    {ACPI_DMT_UINT32,   ACPI_RAS2_PCC_DESC_OFFSET (Instance),               "Instance", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RGRT -  Regulatory Graphics Resource Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoRgrt[] =
{
    {ACPI_DMT_UINT16,   ACPI_RGRT_OFFSET (Version),                 "Version", 0},
    {ACPI_DMT_RGRT,     ACPI_RGRT_OFFSET (ImageType),               "Image Type", 0},
    {ACPI_DMT_UINT8,    ACPI_RGRT_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/*
 * We treat the binary image field as its own subtable (to make
 * ACPI_DMT_RAW_BUFFER work properly).
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoRgrt0[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "Image", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RHCT - RISC-V Hart Capabilities Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoRhct[] =
{
    {ACPI_DMT_UINT32,   ACPI_RHCT_OFFSET (Flags),            "Flags", 0},
    {ACPI_DMT_UINT64,   ACPI_RHCT_OFFSET (TimeBaseFreq),     "Timer Base Frequency", 0},
    {ACPI_DMT_UINT32,   ACPI_RHCT_OFFSET (NodeCount),        "Number of nodes", 0},
    {ACPI_DMT_UINT32,   ACPI_RHCT_OFFSET (NodeOffset),       "Offset to the node array", 0},
    ACPI_DMT_TERMINATOR
};


/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRhctNodeHdr[] =
{
    {ACPI_DMT_RHCT,      ACPI_RHCTH_OFFSET (Type),            "Subtable Type", 0},
    {ACPI_DMT_UINT16,    ACPI_RHCTH_OFFSET (Length),          "Length", 0},
    {ACPI_DMT_UINT16,    ACPI_RHCTH_OFFSET (Revision),        "Revision", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: ISA string type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRhctIsa1[] =
{
    {ACPI_DMT_UINT16,   ACPI_RHCT0_OFFSET (IsaLength),        "ISA string length", 0},
    {ACPI_DMT_STRING,   ACPI_RHCT0_OFFSET (Isa[0]),           "ISA string", 0},
    ACPI_DMT_TERMINATOR
};


/* Optional padding field */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRhctIsaPad[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                  "Optional Padding", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 1: CMO node type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRhctCmo1[] =
{
    {ACPI_DMT_UINT8,   ACPI_RHCT1_OFFSET (Reserved),          "Reserved", 0},
    {ACPI_DMT_UINT8,   ACPI_RHCT1_OFFSET (CbomSize),          "CBOM Block Size", 0},
    {ACPI_DMT_UINT8,   ACPI_RHCT1_OFFSET (CbopSize),          "CBOP Block Size", 0},
    {ACPI_DMT_UINT8,   ACPI_RHCT1_OFFSET (CbozSize),          "CBOZ Block Size", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: MMU node type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRhctMmu1[] =
{
    {ACPI_DMT_UINT8,   ACPI_RHCT2_OFFSET (Reserved),          "Reserved", 0},
    {ACPI_DMT_UINT8,   ACPI_RHCT2_OFFSET (MmuType),           "MMU Type", 0},
    ACPI_DMT_TERMINATOR
};

/* 0xFFFF: Hart Info type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRhctHartInfo1[] =
{
    {ACPI_DMT_UINT16,   ACPI_RHCTFFFF_OFFSET (NumOffsets),    "Number of offsets", 0},
    {ACPI_DMT_UINT32,   ACPI_RHCTFFFF_OFFSET (Uid),           "Processor UID", 0},
    ACPI_DMT_TERMINATOR
};


ACPI_DMTABLE_INFO           AcpiDmTableInfoRhctHartInfo2[] =
{
    {ACPI_DMT_UINT32,   0,                                    "Nodes", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RIMT - RISC-V IO Mapping Table
 *
 * https://github.com/riscv-non-isa/riscv-acpi-rimt
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoRimt[] =
{
    {ACPI_DMT_UINT32,   ACPI_RIMT_OFFSET (NumNodes),              "Number of RIMT Nodes", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMT_OFFSET (NodeOffset),            "Offset to RIMT Node Array", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMT_OFFSET (Reserved),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRimtNodeHdr[] =
{
    {ACPI_DMT_UINT8,     ACPI_RIMTH_OFFSET (Type),                "Type", 0},
    {ACPI_DMT_UINT8,     ACPI_RIMTH_OFFSET (Revision),            "Revision", 0},
    {ACPI_DMT_UINT16,    ACPI_RIMTH_OFFSET (Length),              "Length", 0},
    {ACPI_DMT_UINT16,    ACPI_RIMTH_OFFSET (Reserved),            "Reserved", 0},
    {ACPI_DMT_UINT16,    ACPI_RIMTH_OFFSET (Id),                  "ID", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: IOMMU Node type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRimtIommu[] =
{
    {ACPI_DMT_NAME8,    ACPI_RIMTI_OFFSET (HardwareId),           "Hardware ID", 0},
    {ACPI_DMT_UINT64,   ACPI_RIMTI_OFFSET (BaseAddress),          "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMTI_OFFSET (Flags),                "Flags", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMTI_OFFSET (ProximityDomain),      "Proximity Domain", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTI_OFFSET (PcieSegmentNumber),    "PCIe Segment number", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTI_OFFSET (PcieBdf),              "PCIe B/D/F", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTI_OFFSET (NumInterruptWires),    "Number of interrupt wires", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTI_OFFSET (InterruptWireOffset),  "Interrupt wire array offset", 0},
    ACPI_DMT_TERMINATOR
};


ACPI_DMTABLE_INFO           AcpiDmTableInfoRimtIommuWire[] =
{
    {ACPI_DMT_UINT32,   ACPI_RIMTW_OFFSET (IrqNum),               "Interrupt Number", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMTW_OFFSET (Flags),                "Flags", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: PCIE Root Complex Node type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRimtPcieRc[] =
{
    {ACPI_DMT_UINT32,   ACPI_RIMTP_OFFSET (Flags),               "Flags", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTP_OFFSET (Reserved),            "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTP_OFFSET (PcieSegmentNumber),   "PCIe Segment number", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTP_OFFSET (IdMappingOffset),     "ID mapping array offset", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTP_OFFSET (NumIdMappings),       "Number of ID mappings", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoRimtIdMapping[] =
{
    {ACPI_DMT_UINT32,   ACPI_RIMTM_OFFSET (SourceIdBase),        "Source ID Base", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMTM_OFFSET (NumIds),              "Number of IDs", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMTM_OFFSET (DestIdBase),          "Destination Device ID Base", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMTM_OFFSET (DestOffset),          "Destination IOMMU Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_RIMTM_OFFSET (Flags),               "Flags", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: Platform Device Node type */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRimtPlatDev[] =
{
    {ACPI_DMT_UINT16,   ACPI_RIMTN_OFFSET (IdMappingOffset),     "ID mapping array offset", 0},
    {ACPI_DMT_UINT16,   ACPI_RIMTN_OFFSET (NumIdMappings),       "Number of ID mappings", 0},
    {ACPI_DMT_STRING,   ACPI_RIMTN_OFFSET (DeviceName[0]),       "Device Object Name", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoRimtPlatDevPad[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                     "Padding", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * S3PT - S3 Performance Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoS3pt[] =
{
    {ACPI_DMT_SIG,     ACPI_S3PT_OFFSET (Signature[0]),             "Signature", 0},
    {ACPI_DMT_UINT32,  ACPI_S3PT_OFFSET (Length),                   "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* S3PT subtable header */

ACPI_DMTABLE_INFO           AcpiDmTableInfoS3ptHdr[] =
{
    {ACPI_DMT_UINT16,  ACPI_S3PTH_OFFSET (Type),                    "Type", 0},
    {ACPI_DMT_UINT8,   ACPI_S3PTH_OFFSET (Length),                  "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,   ACPI_S3PTH_OFFSET (Revision),                "Revision", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: Basic S3 Resume Performance Record */

ACPI_DMTABLE_INFO           AcpiDmTableInfoS3pt0[] =
{
    {ACPI_DMT_UINT32,  ACPI_S3PT0_OFFSET (ResumeCount),             "Resume Count", 0},
    {ACPI_DMT_UINT64,  ACPI_S3PT0_OFFSET (FullResume),              "Full Resume", 0},
    {ACPI_DMT_UINT64,  ACPI_S3PT0_OFFSET (AverageResume),           "Average Resume", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: Basic S3 Suspend Performance Record */

ACPI_DMTABLE_INFO           AcpiDmTableInfoS3pt1[] =
{
    {ACPI_DMT_UINT64,  ACPI_S3PT1_OFFSET (SuspendStart),            "Suspend Start", 0},
    {ACPI_DMT_UINT64,  ACPI_S3PT1_OFFSET (SuspendEnd),              "Suspend End", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * SBST - Smart Battery Specification Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoSbst[] =
{
    {ACPI_DMT_UINT32,   ACPI_SBST_OFFSET (WarningLevel),            "Warning Level", 0},
    {ACPI_DMT_UINT32,   ACPI_SBST_OFFSET (LowLevel),                "Low Level", 0},
    {ACPI_DMT_UINT32,   ACPI_SBST_OFFSET (CriticalLevel),           "Critical Level", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * SDEI - Software Delegated Exception Interface Descriptor Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdei[] =
{
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * SDEV - Secure Devices Table (ACPI 6.2)
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdev[] =
{
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdevHdr[] =
{
    {ACPI_DMT_SDEV,     ACPI_SDEVH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEVH_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_SDEVH_FLAG_OFFSET (Flags,0),           "Allow handoff to unsecure OS", 0},
    {ACPI_DMT_FLAG1,    ACPI_SDEVH_FLAG_OFFSET (Flags,0),           "Secure access components present", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEVH_OFFSET (Length),                 "Length",  DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* SDEV Subtables */

/* 0: Namespace Device Based Secure Device Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdev0[] =
{
    {ACPI_DMT_UINT16,   ACPI_SDEV0_OFFSET (DeviceIdOffset),         "Device ID Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV0_OFFSET (DeviceIdLength),         "Device ID Length", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV0_OFFSET (VendorDataOffset),       "Vendor Data Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV0_OFFSET (VendorDataLength),       "Vendor Data Length", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdev0a[] =
{
    {ACPI_DMT_STRING,   0,                                          "Namepath", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdev0b[] =
{
    {ACPI_DMT_UINT16,   ACPI_SDEV0B_OFFSET (SecureComponentOffset), "Secure Access Components Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV0B_OFFSET (SecureComponentLength), "Secure Access Components Length", 0},
    ACPI_DMT_TERMINATOR
};

/* Secure access components */

/* Common secure access components header secure access component */

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdevSecCompHdr[] =
{
    {ACPI_DMT_UINT8,    ACPI_SDEVCH_OFFSET (Type),                   "Secure Component Type", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEVCH_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEVCH_OFFSET (Length),                 "Length", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: Identification Based Secure Access Component */

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdevSecCompId[] =
{
    {ACPI_DMT_UINT16,   ACPI_SDEVC0_OFFSET (HardwareIdOffset),      "Hardware ID Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEVC0_OFFSET (HardwareIdLength),      "Hardware ID Length", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEVC0_OFFSET (SubsystemIdOffset),     "Subsystem ID Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEVC0_OFFSET (SubsystemIdLength),     "Subsystem ID Length", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEVC0_OFFSET (HardwareRevision),      "Hardware Revision", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEVC0_OFFSET (HardwareRevPresent),    "Hardware Rev Present", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEVC0_OFFSET (ClassCodePresent),      "Class Code Present", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEVC0_OFFSET (PciBaseClass),          "PCI Base Class", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEVC0_OFFSET (PciSubClass),           "PCI SubClass", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEVC0_OFFSET (PciProgrammingXface),   "PCI Programming Xface", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: Memory Based Secure Access Component */

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdevSecCompMem[] =
{
    {ACPI_DMT_UINT32,   ACPI_SDEVC1_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_SDEVC1_OFFSET (MemoryBaseAddress),     "Memory Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_SDEVC1_OFFSET (MemoryLength),          "Memory Length", 0},
    ACPI_DMT_TERMINATOR
};


/* 1: PCIe Endpoint Device Based Device Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdev1[] =
{
    {ACPI_DMT_UINT16,   ACPI_SDEV1_OFFSET (Segment),                "Segment", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV1_OFFSET (StartBus),               "Start Bus", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV1_OFFSET (PathOffset),             "Path Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV1_OFFSET (PathLength),             "Path Length", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV1_OFFSET (VendorDataOffset),       "Vendor Data Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_SDEV1_OFFSET (VendorDataLength),       "Vendor Data Length", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdev1a[] =
{
    {ACPI_DMT_UINT8,    ACPI_SDEV1A_OFFSET (Device),                "Device", 0},
    {ACPI_DMT_UINT8,    ACPI_SDEV1A_OFFSET (Function),              "Function", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoSdev1b[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "Vendor Data", 0}, /*, DT_OPTIONAL}, */
    ACPI_DMT_TERMINATOR
};

/*! [End] no source code translation !*/
