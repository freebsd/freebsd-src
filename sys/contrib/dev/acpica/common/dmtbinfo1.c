/******************************************************************************
 *
 * Module Name: dmtbinfo1 - Table info for non-AML tables
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
        ACPI_MODULE_NAME    ("dmtbinfo1")

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
 * ACPI Table Information, used to dump formatted ACPI tables
 *
 * Each entry is of the form:  <Field Type, Field Offset, Field Name>
 */


/*******************************************************************************
 *
 * AEST - ARM Error Source table. Conforms to:
 * ACPI for the Armv8 RAS Extensions 1.1 Platform Design Document Sep 2020
 *
 ******************************************************************************/

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestHdr[] =
{
    {ACPI_DMT_AEST,     ACPI_AESTH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT16,   ACPI_AESTH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_AESTH_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_AESTH_OFFSET (NodeSpecificOffset),     "Node Specific Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_AESTH_OFFSET (NodeInterfaceOffset),    "Node Interface Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_AESTH_OFFSET (NodeInterruptOffset),    "Node Interrupt Array Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_AESTH_OFFSET (NodeInterruptCount),     "Node Interrupt Array Count", 0},
    {ACPI_DMT_UINT64,   ACPI_AESTH_OFFSET (TimestampRate),          "Timestamp Rate", 0},
    {ACPI_DMT_UINT64,   ACPI_AESTH_OFFSET (Reserved1),              "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_AESTH_OFFSET (ErrorInjectionRate),     "Error Injection Rate", 0},
    ACPI_DMT_TERMINATOR
};

/*
 * AEST subtables (nodes)
 */

/* 0: Processor Error */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestProcError[] =
{
    {ACPI_DMT_UINT32,   ACPI_AEST0_OFFSET (ProcessorId),            "Processor ID", 0},
    {ACPI_DMT_AEST_RES, ACPI_AEST0_OFFSET (ResourceType),           "Resource Type", 0},
    {ACPI_DMT_UINT8,    ACPI_AEST0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_AEST0_OFFSET (Flags),                  "Flags (decoded Below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_AEST0_FLAG_OFFSET (Flags, 0),          "Global", 0},
    {ACPI_DMT_FLAG1,    ACPI_AEST0_FLAG_OFFSET (Flags, 0),          "Shared", 0},
    {ACPI_DMT_UINT8,    ACPI_AEST0_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0_OFFSET (ProcessorAffinity),      "Processor Affinity Structure", 0},
    ACPI_DMT_TERMINATOR
};

/* 0RT: Processor Cache Resource */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestCacheRsrc[] =
{
    {ACPI_DMT_UINT32,   ACPI_AEST0A_OFFSET (CacheReference),        "Cache Reference", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0A_OFFSET (Reserved),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 1RT: ProcessorTLB Resource */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestTlbRsrc[] =
{
    {ACPI_DMT_UINT32,   ACPI_AEST0B_OFFSET (TlbLevel),              "TLB Level", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0B_OFFSET (Reserved),              "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 2RT: Processor Generic Resource */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestGenRsrc[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "Resource", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: Memory Error */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestMemError[] =
{
    {ACPI_DMT_UINT32,   ACPI_AEST1_OFFSET (SratProximityDomain),    "Srat Proximity Domain", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: Smmu Error */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestSmmuError[] =
{
    {ACPI_DMT_UINT32,   ACPI_AEST2_OFFSET (IortNodeReference),      "Iort Node Reference", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST2_OFFSET (SubcomponentReference),  "Subcomponent Reference", 0},
    ACPI_DMT_TERMINATOR
};

/* 3: Vendor Defined */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestVendorError[] =
{
    {ACPI_DMT_UINT32,   ACPI_AEST3_OFFSET (AcpiHid),                "ACPI HID", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST3_OFFSET (AcpiUid),                "ACPI UID", 0},
    {ACPI_DMT_BUF16,    ACPI_AEST3_OFFSET (VendorSpecificData),     "Vendor Specific Data", 0},
    ACPI_DMT_TERMINATOR
};

/* 3: Vendor Defined V2 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestVendorV2Error[] =
{
    {ACPI_DMT_UINT64,   ACPI_AEST3A_OFFSET (AcpiHid),                "ACPI HID", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST3A_OFFSET (AcpiUid),                "ACPI UID", 0},
    {ACPI_DMT_BUF16,    ACPI_AEST3A_OFFSET (VendorSpecificData),     "Vendor Specific Data", 0},
    ACPI_DMT_TERMINATOR
};

/* 4: Gic Error */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestGicError[] =
{
    {ACPI_DMT_AEST_GIC, ACPI_AEST4_OFFSET (InterfaceType),          "GIC Interface Type", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST4_OFFSET (InstanceId),             "Instance ID", 0},
    ACPI_DMT_TERMINATOR
};

/* 5: PCIe Error */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestPCIeError[] =
{
    {ACPI_DMT_UINT32,   ACPI_AEST5_OFFSET (IortNodeReference),      "Iort Node Reference", 0},
    ACPI_DMT_TERMINATOR
};

/* 6: Proxy Error */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestProxyError[] =
{
    {ACPI_DMT_UINT64,   ACPI_AEST6_OFFSET (NodeAddress),            "Proxy Node Address", 0},
    ACPI_DMT_TERMINATOR
};

/* Common AEST structures for subtables */

#define ACPI_DM_AEST_INTERFACE_COMMON(a) \
    {ACPI_DMT_UINT32,   ACPI_AEST0D##a##_OFFSET (Common.ErrorNodeDevice),               "Arm Error Node Device", 0},\
    {ACPI_DMT_UINT32,   ACPI_AEST0D##a##_OFFSET (Common.ProcessorAffinity),             "Processor Affinity", 0}, \
    {ACPI_DMT_UINT64,   ACPI_AEST0D##a##_OFFSET (Common.ErrorGroupRegisterBase),        "Err-Group Register Addr", 0}, \
    {ACPI_DMT_UINT64,   ACPI_AEST0D##a##_OFFSET (Common.FaultInjectRegisterBase),       "Err-Inject Register Addr", 0}, \
    {ACPI_DMT_UINT64,   ACPI_AEST0D##a##_OFFSET (Common.InterruptConfigRegisterBase),   "IRQ-Config Register Addr", 0},

/* AestXface: Node Interface Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestXface[] =
{
    {ACPI_DMT_AEST_XFACE, ACPI_AEST0D_OFFSET (Type),                "Interface Type", 0},
    {ACPI_DMT_UINT24,   ACPI_AEST0D_OFFSET (Reserved[0]),           "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0D_OFFSET (Flags),                 "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),         "Shared Interface", 0},
    {ACPI_DMT_FLAG1,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),         "Clear MISCx Registers", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0D_OFFSET (Address),               "Address", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0D_OFFSET (ErrorRecordIndex),      "Error Record Index", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0D_OFFSET (ErrorRecordCount),      "Error Record Count", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0D_OFFSET (ErrorRecordImplemented),"Error Record Implemented", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0D_OFFSET (ErrorStatusReporting),  "Error Status Reporting", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0D_OFFSET (AddressingMode),        "Addressing Mode", 0},
    ACPI_DMT_TERMINATOR
};

/* AestXface: Node Interface Structure V2 Header */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestXfaceHeader[] =
{
    {ACPI_DMT_AEST_XFACE, ACPI_AEST0DH_OFFSET (Type),                "Interface Type", 0},
    {ACPI_DMT_UINT8,    ACPI_AEST0DH_OFFSET (GroupFormat),           "Group Format", 0},
    {ACPI_DMT_UINT16,   ACPI_AEST0DH_OFFSET (Reserved[0]),           "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0DH_OFFSET (Flags),                 "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),          "Shared Interface", 0},
    {ACPI_DMT_FLAG1,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),          "Clear MISCx Registers", 0},
    {ACPI_DMT_FLAG2,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),          "Error Node Device Valid", 0},
    {ACPI_DMT_FLAG3,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),          "Affinity Type", 0},
    {ACPI_DMT_FLAG4,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),          "Error group Address Valid", 0},
    {ACPI_DMT_FLAG5,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),          "Fault Injection Address Valid", 0},
    {ACPI_DMT_FLAG7,    ACPI_AEST0D_FLAG_OFFSET (Flags, 0),          "Interrupt Config Address valid", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0DH_OFFSET (Address),               "Address", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0DH_OFFSET (ErrorRecordIndex),      "Error Record Index", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0DH_OFFSET (ErrorRecordCount),      "Error Record Count", 0},
    ACPI_DMT_TERMINATOR
};

/* AestXface: Node Interface Structure V2 4K Group Format */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestXface4k[] =
{
    {ACPI_DMT_UINT64,   ACPI_AEST0D4_OFFSET (ErrorRecordImplemented),"Error Record Implemented", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0D4_OFFSET (ErrorStatusReporting),  "Error Status Reporting", 0},
    {ACPI_DMT_UINT64,   ACPI_AEST0D4_OFFSET (AddressingMode),        "Addressing Mode", 0},
    ACPI_DM_AEST_INTERFACE_COMMON(4)
    ACPI_DMT_TERMINATOR
};

/* AestXface: Node Interface Structure V2 16K Group Format */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestXface16k[] =
{
    {ACPI_DMT_BUF32,    ACPI_AEST0D16_OFFSET (ErrorRecordImplemented[0]),"Error Record Implemented", 0},
    {ACPI_DMT_BUF32,    ACPI_AEST0D16_OFFSET (ErrorStatusReporting[0]),  "Error Status Reporting", 0},
    {ACPI_DMT_BUF32,    ACPI_AEST0D16_OFFSET (AddressingMode[0]),        "Addressing Mode", 0},
    ACPI_DM_AEST_INTERFACE_COMMON(16)
    ACPI_DMT_TERMINATOR
};

/* AestXface: Node Interface Structure V2 64K Group Format */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestXface64k[] =
{
    {ACPI_DMT_BUF112,   ACPI_AEST0D64_OFFSET (ErrorRecordImplemented[0]),"Error Record Implemented", 0},
    {ACPI_DMT_BUF112,   ACPI_AEST0D64_OFFSET (ErrorStatusReporting[0]),  "Error Status Reporting", 0},
    {ACPI_DMT_BUF112,   ACPI_AEST0D64_OFFSET (AddressingMode[0]),        "Addressing Mode", 0},
    ACPI_DM_AEST_INTERFACE_COMMON(64)
    ACPI_DMT_TERMINATOR
};

/* AestXrupt: Node Interrupt Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestXrupt[] =
{
    {ACPI_DMT_AEST_XRUPT, ACPI_AEST0E_OFFSET (Type),                "Interrupt Type", 0},
    {ACPI_DMT_UINT16,   ACPI_AEST0E_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_AEST0E_OFFSET (Flags),                 "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_AEST0E_FLAG_OFFSET (Flags, 0),         "Level Triggered", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0E_OFFSET (Gsiv),                  "Gsiv", 0},
    {ACPI_DMT_UINT8,    ACPI_AEST0E_OFFSET (IortId),                "IortId", 0},
    {ACPI_DMT_UINT24,   ACPI_AEST0E_OFFSET (Reserved1[0]),          "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/* AestXrupt: Node Interrupt Structure V2 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAestXruptV2[] =
{
    {ACPI_DMT_AEST_XRUPT, ACPI_AEST0EA_OFFSET (Type),                "Interrupt Type", 0},
    {ACPI_DMT_UINT16,   ACPI_AEST0EA_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_AEST0EA_OFFSET (Flags),                 "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_AEST0EA_FLAG_OFFSET (Flags, 0),         "Level Triggered", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0EA_OFFSET (Gsiv),                  "Gsiv", 0},
    {ACPI_DMT_UINT32,   ACPI_AEST0EA_OFFSET (Reserved1[0]),          "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * ASF - Alert Standard Format table (Signature "ASF!")
 *
 ******************************************************************************/

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsfHdr[] =
{
    {ACPI_DMT_ASF,      ACPI_ASF0_OFFSET (Header.Type),             "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF0_OFFSET (Header.Reserved),         "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_ASF0_OFFSET (Header.Length),           "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* 0: ASF Information */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsf0[] =
{
    {ACPI_DMT_UINT8,    ACPI_ASF0_OFFSET (MinResetValue),           "Minimum Reset Value", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF0_OFFSET (MinPollInterval),         "Minimum Polling Interval", 0},
    {ACPI_DMT_UINT16,   ACPI_ASF0_OFFSET (SystemId),                "System ID", 0},
    {ACPI_DMT_UINT32,   ACPI_ASF0_OFFSET (MfgId),                   "Manufacturer ID", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF0_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT24,   ACPI_ASF0_OFFSET (Reserved2[0]),            "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: ASF Alerts */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsf1[] =
{
    {ACPI_DMT_UINT8,    ACPI_ASF1_OFFSET (AssertMask),              "AssertMask", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1_OFFSET (DeassertMask),            "DeassertMask", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1_OFFSET (Alerts),                  "Alert Count", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1_OFFSET (DataLength),              "Alert Data Length", 0},
    ACPI_DMT_TERMINATOR
};

/* 1a: ASF Alert data */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsf1a[] =
{
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Address),                "Address", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Command),                "Command", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Mask),                   "Mask", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Value),                  "Value", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (SensorType),             "SensorType", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Type),                   "Type", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Offset),                 "Offset", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (SourceType),             "SourceType", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Severity),               "Severity", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (SensorNumber),           "SensorNumber", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Entity),                 "Entity", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF1a_OFFSET (Instance),               "Instance", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: ASF Remote Control */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsf2[] =
{
    {ACPI_DMT_UINT8,    ACPI_ASF2_OFFSET (Controls),                "Control Count", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF2_OFFSET (DataLength),              "Control Data Length", 0},
    {ACPI_DMT_UINT16,   ACPI_ASF2_OFFSET (Reserved2),               "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* 2a: ASF Control data */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsf2a[] =
{
    {ACPI_DMT_UINT8,    ACPI_ASF2a_OFFSET (Function),               "Function", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF2a_OFFSET (Address),                "Address", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF2a_OFFSET (Command),                "Command", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF2a_OFFSET (Value),                  "Value", 0},
    ACPI_DMT_TERMINATOR
};

/* 3: ASF RMCP Boot Options */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsf3[] =
{
    {ACPI_DMT_BUF7,     ACPI_ASF3_OFFSET (Capabilities[0]),         "Capabilities", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF3_OFFSET (CompletionCode),          "Completion Code", 0},
    {ACPI_DMT_UINT32,   ACPI_ASF3_OFFSET (EnterpriseId),            "Enterprise ID", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF3_OFFSET (Command),                 "Command", 0},
    {ACPI_DMT_UINT16,   ACPI_ASF3_OFFSET (Parameter),               "Parameter", 0},
    {ACPI_DMT_UINT16,   ACPI_ASF3_OFFSET (BootOptions),             "Boot Options", 0},
    {ACPI_DMT_UINT16,   ACPI_ASF3_OFFSET (OemParameters),           "Oem Parameters", 0},
    ACPI_DMT_TERMINATOR
};

/* 4: ASF Address */

ACPI_DMTABLE_INFO           AcpiDmTableInfoAsf4[] =
{
    {ACPI_DMT_UINT8,    ACPI_ASF4_OFFSET (EpromAddress),            "Eprom Address", 0},
    {ACPI_DMT_UINT8,    ACPI_ASF4_OFFSET (Devices),                 "Device Count", DT_COUNT},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * ASPT - AMD Secure Processor table (Signature "ASPT")
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoAspt[] =
{
    {ACPI_DMT_UINT32,   ACPI_ASPT_OFFSET(NumEntries),               "Number of Subtables", 0},
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header (one per Subtable) */
ACPI_DMTABLE_INFO           AcpiDmTableInfoAsptHdr[] =
{
    {ACPI_DMT_ASPT,     ACPI_ASPTH_OFFSET(Type),                    "Type", 0},
    {ACPI_DMT_UINT16,   ACPI_ASPTH_OFFSET(Length),                  "Length", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: ASPT Global Registers */
ACPI_DMTABLE_INFO AcpiDmTableInfoAspt0[] =
{
    {ACPI_DMT_UINT32,   ACPI_ASPT0_OFFSET(Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT0_OFFSET(FeatureRegAddr),          "Feature Register Address", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT0_OFFSET(IrqEnRegAddr),            "Interrupt Enable Register Address", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT0_OFFSET(IrqStRegAddr),            "Interrupt Status Register Address", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: ASPT SEV Mailbox Registers */
ACPI_DMTABLE_INFO AcpiDmTableInfoAspt1[] =
{
    {ACPI_DMT_UINT8,    ACPI_ASPT1_OFFSET(MboxIrqId),               "Mailbox Interrupt ID", 0},
    {ACPI_DMT_UINT24,   ACPI_ASPT1_OFFSET(Reserved[0]),             "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT1_OFFSET(CmdRespRegAddr),          "CmdResp Register Address", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT1_OFFSET(CmdBufLoRegAddr),         "CmdBufAddr_Lo Register Address", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT1_OFFSET(CmdBufHiRegAddr),         "CmdBufAddr_Hi Register Address", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: ASPT ACPI Maiblox Registers */
ACPI_DMTABLE_INFO AcpiDmTableInfoAspt2[] =
{
    {ACPI_DMT_UINT32,   ACPI_ASPT2_OFFSET(Reserved1),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT2_OFFSET(CmdRespRegAddr),          "CmdResp Register Address", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT2_OFFSET(Reserved2[0]),           "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ASPT2_OFFSET(Reserved2[1]),           "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/*******************************************************************************
 *
 * BDAT -  BIOS Data ACPI Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoBdat[] =
{
    {ACPI_DMT_GAS,      ACPI_BDAT_OFFSET (Gas),                     "BDAT Generic Address", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * BERT -  Boot Error Record table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoBert[] =
{
    {ACPI_DMT_UINT32,   ACPI_BERT_OFFSET (RegionLength),            "Boot Error Region Length", 0},
    {ACPI_DMT_UINT64,   ACPI_BERT_OFFSET (Address),                 "Boot Error Region Address", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * BGRT -  Boot Graphics Resource Table (ACPI 5.0)
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoBgrt[] =
{
    {ACPI_DMT_UINT16,   ACPI_BGRT_OFFSET (Version),                 "Version", 0},
    {ACPI_DMT_UINT8,    ACPI_BGRT_OFFSET (Status),                  "Status (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_BGRT_FLAG_OFFSET (Status, 0),          "Displayed", 0},
    {ACPI_DMT_FLAGS1,   ACPI_BGRT_FLAG_OFFSET (Status, 0),          "Orientation Offset", 0},

    {ACPI_DMT_UINT8,    ACPI_BGRT_OFFSET (ImageType),               "Image Type", 0},
    {ACPI_DMT_UINT64,   ACPI_BGRT_OFFSET (ImageAddress),            "Image Address", 0},
    {ACPI_DMT_UINT32,   ACPI_BGRT_OFFSET (ImageOffsetX),            "Image OffsetX", 0},
    {ACPI_DMT_UINT32,   ACPI_BGRT_OFFSET (ImageOffsetY),            "Image OffsetY", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * BOOT - Simple Boot Flag Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoBoot[] =
{
    {ACPI_DMT_UINT8,    ACPI_BOOT_OFFSET (CmosIndex),               "Boot Register Index", 0},
    {ACPI_DMT_UINT24,   ACPI_BOOT_OFFSET (Reserved[0]),             "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/*******************************************************************************
 *
 * CDAT - Coherent Device Attribute Table
 *
 ******************************************************************************/

 /* Table header (not ACPI-compliant) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdatTableHdr[] =
{
    {ACPI_DMT_UINT32,   ACPI_CDAT_OFFSET (Length),              "CDAT Table Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_CDAT_OFFSET (Revision),            "Revision", 0},
    {ACPI_DMT_UINT8,    ACPI_CDAT_OFFSET (Checksum),            "Checksum", 0},
    {ACPI_DMT_UINT48,   ACPI_CDAT_OFFSET (Reserved),            "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_CDAT_OFFSET (Sequence),            "Sequence", 0},
    ACPI_DMT_TERMINATOR
};

/* Common subtable header */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdatHeader[] =
{
    {ACPI_DMT_CDAT,     ACPI_CDATH_OFFSET (Type),               "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_CDATH_OFFSET (Reserved),           "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_CDATH_OFFSET (Length),             "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* Subtable 0: Device Scoped Memory Affinity Structure (DSMAS) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdat0[] =
{
    {ACPI_DMT_UINT8,    ACPI_CDAT0_OFFSET (DsmadHandle),        "DSMAD Handle", 0},
    {ACPI_DMT_UINT8,    ACPI_CDAT0_OFFSET (Flags),              "Flags", 0},
    {ACPI_DMT_UINT16,   ACPI_CDAT0_OFFSET (Reserved),           "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_CDAT0_OFFSET (DpaBaseAddress),     "DPA Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_CDAT0_OFFSET (DpaLength),          "DPA Length", 0},
    ACPI_DMT_TERMINATOR
};

/* Subtable 1: Device scoped Latency and Bandwidth Information Structure (DSLBIS) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdat1[] =
{
    {ACPI_DMT_UINT8,    ACPI_CDAT1_OFFSET (Handle),             "Handle", 0},
    {ACPI_DMT_UINT8,    ACPI_CDAT1_OFFSET (Flags),              "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_CDAT1_OFFSET (DataType),           "Data Type", 0},
    {ACPI_DMT_UINT8,    ACPI_CDAT1_OFFSET (Reserved),           "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_CDAT1_OFFSET (EntryBaseUnit),      "Entry Base Unit", 0},
    {ACPI_DMT_UINT16,   ACPI_CDAT1_OFFSET (Entry[0]),           "Entry0", 0},
    {ACPI_DMT_UINT16,   ACPI_CDAT1_OFFSET (Entry[1]),           "Entry1", 0},
    {ACPI_DMT_UINT16,   ACPI_CDAT1_OFFSET (Entry[2]),           "Entry2", 0},
    {ACPI_DMT_UINT16,   ACPI_CDAT1_OFFSET (Reserved2),          "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* Subtable 2: Device Scoped Memory Side Cache Information Structure (DSMSCIS) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdat2[] =
{
    {ACPI_DMT_UINT8,    ACPI_CDAT2_OFFSET (DsmasHandle),        "DSMAS Handle", 0},
    {ACPI_DMT_UINT24,   ACPI_CDAT2_OFFSET (Reserved[3]),        "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_CDAT2_OFFSET (SideCacheSize),      "Side Cache Size", 0},
    {ACPI_DMT_UINT32,   ACPI_CDAT2_OFFSET (CacheAttributes),    "Cache Attributes", 0},
    ACPI_DMT_TERMINATOR
};

/* Subtable 3: Device Scoped Initiator Structure (DSIS) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdat3[] =
{
    {ACPI_DMT_UINT8,    ACPI_CDAT3_OFFSET (Flags),              "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_CDAT3_OFFSET (Handle),             "Handle", 0},
    {ACPI_DMT_UINT16,   ACPI_CDAT3_OFFSET (Reserved),           "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* Subtable 4: Device Scoped EFI Memory Type Structure (DSEMTS) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdat4[] =
{
    {ACPI_DMT_UINT8,    ACPI_CDAT4_OFFSET (DsmasHandle),        "DSMAS Handle", 0},
    {ACPI_DMT_UINT8,    ACPI_CDAT4_OFFSET (MemoryType),         "Memory Type", 0},
    {ACPI_DMT_UINT16,   ACPI_CDAT4_OFFSET (Reserved),           "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_CDAT4_OFFSET (DpaOffset),          "DPA Offset", 0},
    {ACPI_DMT_UINT64,   ACPI_CDAT4_OFFSET (RangeLength),        "DPA Range Length", 0},
    ACPI_DMT_TERMINATOR
};

/* Subtable 5: Switch Scoped Latency and Bandwidth Information Structure (SSLBIS) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdat5[] =
{
    {ACPI_DMT_UINT8,    ACPI_CDAT5_OFFSET (DataType),           "Data Type", 0},
    {ACPI_DMT_UINT24,   ACPI_CDAT5_OFFSET (Reserved),           "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_CDAT5_OFFSET (EntryBaseUnit),      "Entry Base Unit", 0},
    ACPI_DMT_TERMINATOR
};

/* Switch Scoped Latency and Bandwidth Entry (SSLBE) (For subtable 5 above) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCdatEntries[] =
{
    {ACPI_DMT_UINT16,   ACPI_CDATE_OFFSET (PortxId),            "Port X Id", 0},
    {ACPI_DMT_UINT16,   ACPI_CDATE_OFFSET (PortyId),            "Port Y Id", 0},
    {ACPI_DMT_UINT16,   ACPI_CDATE_OFFSET (LatencyOrBandwidth), "Latency or Bandwidth", 0},
    {ACPI_DMT_UINT16,   ACPI_CDATE_OFFSET (Reserved),           "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * CEDT - CXL Early Discovery Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoCedtHdr[] =
{
    {ACPI_DMT_CEDT,     ACPI_CEDT_OFFSET (Type),               "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_CEDT_OFFSET (Reserved),           "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_CEDT_OFFSET (Length),             "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* 0: CXL Host Bridge Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCedt0[] =
{
    {ACPI_DMT_UINT32,   ACPI_CEDT0_OFFSET (Uid),               "Associated host bridge", 0},
    {ACPI_DMT_UINT32,   ACPI_CEDT0_OFFSET (CxlVersion),        "Specification version", 0},
    {ACPI_DMT_UINT32,   ACPI_CEDT0_OFFSET (Reserved),          "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_CEDT0_OFFSET (Base),              "Register base", 0},
    {ACPI_DMT_UINT64,   ACPI_CEDT0_OFFSET (Length),            "Register length", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: CXL Fixed Memory Window Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCedt1[] =
{
    {ACPI_DMT_UINT32,   ACPI_CEDT1_OFFSET (Reserved1),            "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_CEDT1_OFFSET (BaseHpa),              "Window base address", 0},
    {ACPI_DMT_UINT64,   ACPI_CEDT1_OFFSET (WindowSize),           "Window size", 0},
    {ACPI_DMT_UINT8,    ACPI_CEDT1_OFFSET (InterleaveWays),       "Interleave Members", 0},
    {ACPI_DMT_UINT8,    ACPI_CEDT1_OFFSET (InterleaveArithmetic), "Interleave Arithmetic", 0},
    {ACPI_DMT_UINT16,   ACPI_CEDT1_OFFSET (Reserved2),            "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_CEDT1_OFFSET (Granularity),          "Granularity", 0},
    {ACPI_DMT_UINT16,   ACPI_CEDT1_OFFSET (Restrictions),         "Restrictions", 0},
    {ACPI_DMT_UINT16,   ACPI_CEDT1_OFFSET (QtgId),                "QtgId", 0},
    {ACPI_DMT_UINT32,   ACPI_CEDT1_OFFSET (InterleaveTargets),    "First Target", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoCedt1_te[] =
{
    {ACPI_DMT_UINT32,   ACPI_CEDT1_TE_OFFSET (InterleaveTarget),  "Next Target", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: CXL XOR Interleave Math Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCedt2[] =
{
    {ACPI_DMT_UINT16,   ACPI_CEDT2_OFFSET (Reserved1),            "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_CEDT2_OFFSET (Hbig),                 "Interleave Granularity", 0},
    {ACPI_DMT_UINT8,    ACPI_CEDT2_OFFSET (NrXormaps),            "Xormap List Count", 0},
    {ACPI_DMT_UINT64,   ACPI_CEDT2_OFFSET (XormapList),           "First Xormap", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoCedt2_te[] =
{
    {ACPI_DMT_UINT64,   ACPI_CEDT2_TE_OFFSET (Xormap),            "Next Xormap", 0},
    ACPI_DMT_TERMINATOR
};

/*******************************************************************************
 *
 * CPEP - Corrected Platform Error Polling table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoCpep[] =
{
    {ACPI_DMT_UINT64,   ACPI_CPEP_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoCpep0[] =
{
    {ACPI_DMT_UINT8,    ACPI_CPEP0_OFFSET (Header.Type),            "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_CPEP0_OFFSET (Header.Length),          "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_CPEP0_OFFSET (Id),                     "Processor ID", 0},
    {ACPI_DMT_UINT8,    ACPI_CPEP0_OFFSET (Eid),                    "Processor EID", 0},
    {ACPI_DMT_UINT32,   ACPI_CPEP0_OFFSET (Interval),               "Polling Interval", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * CSRT - Core System Resource Table
 *
 ******************************************************************************/

/* Main table consists only of the standard ACPI table header */

/* Resource Group subtable */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCsrt0[] =
{
    {ACPI_DMT_UINT32,   ACPI_CSRT0_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT32,   ACPI_CSRT0_OFFSET (VendorId),               "Vendor ID", 0},
    {ACPI_DMT_UINT32,   ACPI_CSRT0_OFFSET (SubvendorId),            "Subvendor ID", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT0_OFFSET (DeviceId),               "Device ID", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT0_OFFSET (SubdeviceId),            "Subdevice ID", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT0_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_CSRT0_OFFSET (SharedInfoLength),       "Shared Info Length", 0},
    ACPI_DMT_TERMINATOR
};

/* Shared Info subtable */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCsrt1[] =
{
    {ACPI_DMT_UINT16,   ACPI_CSRT1_OFFSET (MajorVersion),           "Major Version", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT1_OFFSET (MinorVersion),           "Minor Version", 0},
    {ACPI_DMT_UINT32,   ACPI_CSRT1_OFFSET (MmioBaseLow),            "MMIO Base Address Low", 0},
    {ACPI_DMT_UINT32,   ACPI_CSRT1_OFFSET (MmioBaseHigh),           "MMIO Base Address High", 0},
    {ACPI_DMT_UINT32,   ACPI_CSRT1_OFFSET (GsiInterrupt),           "GSI Interrupt", 0},
    {ACPI_DMT_UINT8,    ACPI_CSRT1_OFFSET (InterruptPolarity),      "Interrupt Polarity", 0},
    {ACPI_DMT_UINT8,    ACPI_CSRT1_OFFSET (InterruptMode),          "Interrupt Mode", 0},
    {ACPI_DMT_UINT8,    ACPI_CSRT1_OFFSET (NumChannels),            "Num Channels", 0},
    {ACPI_DMT_UINT8,    ACPI_CSRT1_OFFSET (DmaAddressWidth),        "DMA Address Width", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT1_OFFSET (BaseRequestLine),        "Base Request Line", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT1_OFFSET (NumHandshakeSignals),    "Num Handshake Signals", 0},
    {ACPI_DMT_UINT32,   ACPI_CSRT1_OFFSET (MaxBlockSize),           "Max Block Size", 0},
    ACPI_DMT_TERMINATOR
};

/* Resource Descriptor subtable */

ACPI_DMTABLE_INFO           AcpiDmTableInfoCsrt2[] =
{
    {ACPI_DMT_UINT32,   ACPI_CSRT2_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT16,   ACPI_CSRT2_OFFSET (Type),                   "Type", 0},
    {ACPI_DMT_UINT16,   ACPI_CSRT2_OFFSET (Subtype),                "Subtype", 0},
    {ACPI_DMT_UINT32,   ACPI_CSRT2_OFFSET (Uid),                    "UID", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoCsrt2a[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "ResourceInfo", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * DBG2 - Debug Port Table 2
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoDbg2[] =
{
    {ACPI_DMT_UINT32,   ACPI_DBG2_OFFSET (InfoOffset),              "Info Offset", 0},
    {ACPI_DMT_UINT32,   ACPI_DBG2_OFFSET (InfoCount),               "Info Count", 0},
    ACPI_DMT_TERMINATOR
};

/* Debug Device Information Subtable */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDbg2Device[] =
{
    {ACPI_DMT_UINT8,    ACPI_DBG20_OFFSET (Revision),               "Revision", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_DBG20_OFFSET (RegisterCount),          "Register Count", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (NamepathLength),         "Namepath Length", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (NamepathOffset),         "Namepath Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (OemDataLength),          "OEM Data Length", DT_DESCRIBES_OPTIONAL},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (OemDataOffset),          "OEM Data Offset", DT_DESCRIBES_OPTIONAL},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (PortType),               "Port Type", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (PortSubtype),            "Port Subtype", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (BaseAddressOffset),      "Base Address Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_DBG20_OFFSET (AddressSizeOffset),      "Address Size Offset", 0},
    ACPI_DMT_TERMINATOR
};

/* Variable-length data for the subtable */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDbg2Addr[] =
{
    {ACPI_DMT_GAS,      0,                                          "Base Address Register", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDbg2Size[] =
{
    {ACPI_DMT_UINT32,   0,                                          "Address Size", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDbg2Name[] =
{
    {ACPI_DMT_STRING,   0,                                          "Namepath", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDbg2OemData[] =
{
    {ACPI_DMT_RAW_BUFFER, 0,                                        "OEM Data", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * DBGP - Debug Port
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoDbgp[] =
{
    {ACPI_DMT_UINT8,    ACPI_DBGP_OFFSET (Type),                    "Interface Type", 0},
    {ACPI_DMT_UINT24,   ACPI_DBGP_OFFSET (Reserved[0]),             "Reserved", 0},
    {ACPI_DMT_GAS,      ACPI_DBGP_OFFSET (DebugPort),               "Debug Port Register", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * DMAR - DMA Remapping table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar[] =
{
    {ACPI_DMT_UINT8,    ACPI_DMAR_OFFSET (Width),                   "Host Address Width", 0},
    {ACPI_DMT_UINT8,    ACPI_DMAR_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_BUF10,    ACPI_DMAR_OFFSET (Reserved[0]),             "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* Common Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmarHdr[] =
{
    {ACPI_DMT_DMAR,     ACPI_DMAR0_OFFSET (Header.Type),            "Subtable Type", 0},
    {ACPI_DMT_UINT16,   ACPI_DMAR0_OFFSET (Header.Length),          "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* Common device scope entry */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmarScope[] =
{
    {ACPI_DMT_DMAR_SCOPE, ACPI_DMARS_OFFSET (EntryType),            "Device Scope Type", 0},
    {ACPI_DMT_UINT8,    ACPI_DMARS_OFFSET (Length),                 "Entry Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_DMARS_OFFSET (Flags),                  "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_DMARS_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_DMARS_OFFSET (EnumerationId),          "Enumeration ID", 0},
    {ACPI_DMT_UINT8,    ACPI_DMARS_OFFSET (Bus),                    "PCI Bus Number", 0},
    ACPI_DMT_TERMINATOR
};

/* DMAR Subtables */

/* 0: Hardware Unit Definition */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar0[] =
{
    {ACPI_DMT_UINT8,    ACPI_DMAR0_OFFSET (Flags),                  "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_DMAR0_OFFSET (Size),                   "Size (decoded below)", 0},
    {ACPI_DMT_FLAGS4_0, ACPI_DMAR0_FLAG_OFFSET (Size,0),            "Size (pages, log2)", 0},
    {ACPI_DMT_UINT16,   ACPI_DMAR0_OFFSET (Segment),                "PCI Segment Number", 0},
    {ACPI_DMT_UINT64,   ACPI_DMAR0_OFFSET (Address),                "Register Base Address", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: Reserved Memory Definition */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar1[] =
{
    {ACPI_DMT_UINT16,   ACPI_DMAR1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_DMAR1_OFFSET (Segment),                "PCI Segment Number", 0},
    {ACPI_DMT_UINT64,   ACPI_DMAR1_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_DMAR1_OFFSET (EndAddress),             "End Address (limit)", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: Root Port ATS Capability Definition */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar2[] =
{
    {ACPI_DMT_UINT8,    ACPI_DMAR2_OFFSET (Flags),                  "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_DMAR2_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_DMAR2_OFFSET (Segment),                "PCI Segment Number", 0},
    ACPI_DMT_TERMINATOR
};

/* 3: Remapping Hardware Static Affinity Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar3[] =
{
    {ACPI_DMT_UINT32,   ACPI_DMAR3_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_DMAR3_OFFSET (BaseAddress),            "Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_DMAR3_OFFSET (ProximityDomain),        "Proximity Domain", 0},
    ACPI_DMT_TERMINATOR
};

/* 4: ACPI Namespace Device Declaration Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar4[] =
{
    {ACPI_DMT_UINT24,   ACPI_DMAR4_OFFSET (Reserved[0]),            "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_DMAR4_OFFSET (DeviceNumber),           "Device Number", 0},
    {ACPI_DMT_STRING,   ACPI_DMAR4_OFFSET (DeviceName[0]),          "Device Name", 0},
    ACPI_DMT_TERMINATOR
};

/* 5: SoC Integrated Address Translation Cache */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar5[] =
{
    {ACPI_DMT_UINT8,    ACPI_DMAR5_OFFSET (Flags),                  "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_DMAR5_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_DMAR5_OFFSET (Segment),                "PCI Segment Number", 0},
    ACPI_DMT_TERMINATOR
};

/* 6: SoC Integrated Device Property */

ACPI_DMTABLE_INFO           AcpiDmTableInfoDmar6[] =
{
    {ACPI_DMT_UINT16,   ACPI_DMAR6_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_DMAR6_OFFSET (Segment),                "PCI Segment Number", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * DRTM - Dynamic Root of Trust for Measurement table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoDrtm[] =
{
    {ACPI_DMT_UINT64,   ACPI_DRTM_OFFSET (EntryBaseAddress),        "Entry Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_DRTM_OFFSET (EntryLength),             "Entry Length", 0},
    {ACPI_DMT_UINT32,   ACPI_DRTM_OFFSET (EntryAddress32),          "Entry 32", 0},
    {ACPI_DMT_UINT64,   ACPI_DRTM_OFFSET (EntryAddress64),          "Entry 64", 0},
    {ACPI_DMT_UINT64,   ACPI_DRTM_OFFSET (ExitAddress),             "Exit Address", 0},
    {ACPI_DMT_UINT64,   ACPI_DRTM_OFFSET (LogAreaAddress),          "Log Area Start", 0},
    {ACPI_DMT_UINT32,   ACPI_DRTM_OFFSET (LogAreaLength),           "Log Area Length", 0},
    {ACPI_DMT_UINT64,   ACPI_DRTM_OFFSET (ArchDependentAddress),    "Arch Dependent Address", 0},
    {ACPI_DMT_UINT32,   ACPI_DRTM_OFFSET (Flags),                   "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_DRTM_FLAG_OFFSET (Flags, 0),           "Namespace in TCB", 0},
    {ACPI_DMT_FLAG1,    ACPI_DRTM_FLAG_OFFSET (Flags, 0),           "Gap Code on S3 Resume", 0},
    {ACPI_DMT_FLAG2,    ACPI_DRTM_FLAG_OFFSET (Flags, 0),           "Gap Code on DLME_Exit", 0},
    {ACPI_DMT_FLAG3,    ACPI_DRTM_FLAG_OFFSET (Flags, 0),           "PCR_Authorities Changed", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDrtm0[] =
{
    {ACPI_DMT_UINT32,   ACPI_DRTM0_OFFSET (ValidatedTableCount),    "Validated Table Count", DT_COUNT},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDrtm0a[] =
{
    {ACPI_DMT_UINT64,   0,                                          "Table Address", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDrtm1[] =
{
    {ACPI_DMT_UINT32,   ACPI_DRTM1_OFFSET (ResourceCount),          "Resource Count", DT_COUNT},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDrtm1a[] =
{
    {ACPI_DMT_UINT56,   ACPI_DRTM1a_OFFSET (Size[0]),               "Size", DT_OPTIONAL},
    {ACPI_DMT_UINT8,    ACPI_DRTM1a_OFFSET (Type),                  "Type", 0},
    {ACPI_DMT_FLAG0,    ACPI_DRTM1a_FLAG_OFFSET (Type, 0),          "Resource Type", 0},
    {ACPI_DMT_FLAG7,    ACPI_DRTM1a_FLAG_OFFSET (Type, 0),          "Protections", 0},
    {ACPI_DMT_UINT64,   ACPI_DRTM1a_OFFSET (Address),               "Address", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoDrtm2[] =
{
    {ACPI_DMT_UINT32,   ACPI_DRTM2_OFFSET (DpsIdLength),            "DLME Platform Id Length", DT_COUNT},
    {ACPI_DMT_BUF16,    ACPI_DRTM2_OFFSET (DpsId),                  "DLME Platform Id", DT_COUNT},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * ECDT - Embedded Controller Boot Resources Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoEcdt[] =
{
    {ACPI_DMT_GAS,      ACPI_ECDT_OFFSET (Control),                 "Command/Status Register", 0},
    {ACPI_DMT_GAS,      ACPI_ECDT_OFFSET (Data),                    "Data Register", 0},
    {ACPI_DMT_UINT32,   ACPI_ECDT_OFFSET (Uid),                     "UID", 0},
    {ACPI_DMT_UINT8,    ACPI_ECDT_OFFSET (Gpe),                     "GPE Number", 0},
    {ACPI_DMT_STRING,   ACPI_ECDT_OFFSET (Id[0]),                   "Namepath", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * EINJ - Error Injection table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoEinj[] =
{
    {ACPI_DMT_UINT32,   ACPI_EINJ_OFFSET (HeaderLength),            "Injection Header Length", 0},
    {ACPI_DMT_UINT8,    ACPI_EINJ_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT24,   ACPI_EINJ_OFFSET (Reserved[0]),             "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_EINJ_OFFSET (Entries),                 "Injection Entry Count", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoEinj0[] =
{
    {ACPI_DMT_EINJACT,  ACPI_EINJ0_OFFSET (Action),                 "Action", 0},
    {ACPI_DMT_EINJINST, ACPI_EINJ0_OFFSET (Instruction),            "Instruction", 0},
    {ACPI_DMT_UINT8,    ACPI_EINJ0_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_EINJ0_FLAG_OFFSET (Flags,0),           "Preserve Register Bits", 0},

    {ACPI_DMT_UINT8,    ACPI_EINJ0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_GAS,      ACPI_EINJ0_OFFSET (RegisterRegion),         "Register Region", 0},
    {ACPI_DMT_UINT64,   ACPI_EINJ0_OFFSET (Value),                  "Value", 0},
    {ACPI_DMT_UINT64,   ACPI_EINJ0_OFFSET (Mask),                   "Mask", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * ERDT - Enhanced Resource Director Technology table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdt[] =
{
    {ACPI_DMT_UINT32,   ACPI_ERDT_OFFSET (MaxClos),                 "Maximum supported CLOSID", 0},
    {ACPI_DMT_BUF24,    ACPI_ERDT_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * ERDT - Common Subtable Header
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtHdr[] =
{
    {ACPI_DMT_ERDT,     ACPI_ERDT_HDR_OFFSET (Type),                "Type", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_HDR_OFFSET (Length),              "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - ERDT Resource Management Domain Description subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtRmdd[] =
{
    {ACPI_DMT_UINT16,   ACPI_ERDT_RMDD_OFFSET      (Flags),              "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_ERDT_RMDD_FLAG_OFFSET (Flags,0),            "L3 Domain", 0},
    {ACPI_DMT_FLAG1,    ACPI_ERDT_RMDD_FLAG_OFFSET (Flags,0),            "I/O L3 Domain", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_RMDD_OFFSET      (IO_l3_Slices),       "I/O L3 Slices", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_RMDD_OFFSET      (IO_l3_Sets),         "I/O L3 Sets", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_RMDD_OFFSET      (IO_l3_Ways),         "I/O L3 Ways", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_RMDD_OFFSET      (Reserved),           "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_RMDD_OFFSET      (DomainId),           "Domain ID", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_RMDD_OFFSET      (MaxRmid),            "Maximum supported RMID", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_RMDD_OFFSET      (CregBase),           "Control Register Base Address", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_RMDD_OFFSET      (CregSize),           "Control Register Base Size", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - CACD CPU Agent Collection Description subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtCacd[] =
{
    {ACPI_DMT_UINT16,   ACPI_ERDT_CACD_OFFSET (Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_CACD_OFFSET (DomainId),                "Domain ID", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtCacdX2apic[] =
{
    {ACPI_DMT_UINT32,   0,                                               "X2ApicID", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - DACD Device Agent Collection Description subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtDacd[] =
{
    {ACPI_DMT_UINT16,   ACPI_ERDT_DACD_OFFSET (Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_DACD_OFFSET (DomainId),                "Domain ID", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtDacdScope[] =
{
    {ACPI_DMT_UINT8,    ACPI_ERDT_DACD_PATH_OFFSET (Header.Type),        "PCIType", DT_OPTIONAL},
    {ACPI_DMT_UINT8,    ACPI_ERDT_DACD_PATH_OFFSET (Header.Length),      "Length", DT_OPTIONAL},
    {ACPI_DMT_UINT16,   ACPI_ERDT_DACD_PATH_OFFSET (Segment),            "Segment", DT_OPTIONAL},
    {ACPI_DMT_UINT8,    ACPI_ERDT_DACD_PATH_OFFSET (Reserved),           "Reserved", DT_OPTIONAL},
    {ACPI_DMT_UINT8,    ACPI_ERDT_DACD_PATH_OFFSET (StartBus),           "StartBus", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtDacdPath[] =
{
    {ACPI_DMT_UINT8,    0,                                               "Path", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - Cache Monitoring Registers for CPU Agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtCmrc[] =
{
    {ACPI_DMT_UINT32,   ACPI_ERDT_CMRC_OFFSET (Reserved1),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_CMRC_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_CMRC_OFFSET (IndexFn),                 "Register Index Function", 0},
    {ACPI_DMT_BUF11,    ACPI_ERDT_CMRC_OFFSET (Reserved2),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_CMRC_OFFSET (CmtRegBase),              "CMT Register Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_CMRC_OFFSET (CmtRegSize),              "CMT Register Size", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_CMRC_OFFSET (ClumpSize),               "Clump Size", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_CMRC_OFFSET (ClumpStride),             "Clump Stride", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_CMRC_OFFSET (UpScale),                 "Upscale factor", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - Memory-bandwidth Monitoring Registers for CPU agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtMmrc[] =
{
    {ACPI_DMT_UINT32,   ACPI_ERDT_MMRC_OFFSET (Reserved1),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_MMRC_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_MMRC_OFFSET (IndexFn),                 "Register Index Function", 0},
    {ACPI_DMT_BUF11,    ACPI_ERDT_MMRC_OFFSET (Reserved2),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_MMRC_OFFSET (RegBase),                 "MBM Register Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_MMRC_OFFSET (RegSize),                 "MBM Register Size", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_MMRC_OFFSET (CounterWidth),            "MBM Counter Width", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_MMRC_OFFSET (UpScale),                 "Upscale factor", 0},
    {ACPI_DMT_UINT56,   ACPI_ERDT_MMRC_OFFSET (Reserved3),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_MMRC_OFFSET (CorrFactorListLen),       "Corr Factor List Length", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtMmrcCorrFactor[] =
{
    {ACPI_DMT_UINT32,   0,                                               "CorrFactor", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - Memory-bandwidth Allocation Registers for CPU agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtMarc[] =
{
    {ACPI_DMT_UINT16,   ACPI_ERDT_MARC_OFFSET (Reserved1),               "Reserved", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_MARC_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_MARC_OFFSET (IndexFn),                 "Register Index Function", 0},
    {ACPI_DMT_UINT56,   ACPI_ERDT_MARC_OFFSET (Reserved2),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_MARC_OFFSET (RegBaseOpt),              "MBA Register Opt Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_MARC_OFFSET (RegBaseMin),              "MBA Register Min Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_MARC_OFFSET (RegBaseMax),              "MBA Register Max Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_MARC_OFFSET (MbaRegSize),              "MBA Register Size", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_MARC_OFFSET (MbaCtrlRange),            "MBA Control Range", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - Cache Allocation Registers for CPU Agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtCarc[] =
{
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - Cache Monitoring Registers for Device Agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtCmrd[] =
{
    {ACPI_DMT_UINT32,   ACPI_ERDT_CMRD_OFFSET (Reserved1),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_CMRD_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_CMRD_OFFSET (IndexFn),                 "Register Index Function", 0},
    {ACPI_DMT_BUF11,    ACPI_ERDT_CMRD_OFFSET (Reserved2),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_CMRD_OFFSET (RegBase),                 "CMRD Register Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_CMRD_OFFSET (RegSize),                 "CMRD Register Size", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_CMRD_OFFSET (CmtRegOff),               "Register Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_CMRD_OFFSET (CmtClumpSize),            "Clump Size", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_CMRD_OFFSET (UpScale),                 "Upscale factor", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - O Bandwidth Monitoring Registers for Device Agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtIbrd[] =
{
    {ACPI_DMT_UINT32,   ACPI_ERDT_IBRD_OFFSET (Reserved1),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_IBRD_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_IBRD_OFFSET (IndexFn),                 "Register Index Function", 0},
    {ACPI_DMT_BUF11,    ACPI_ERDT_IBRD_OFFSET (Reserved2),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_IBRD_OFFSET (RegBase),                 "IBRD Register Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_IBRD_OFFSET (RegSize),                 "IBRD Register Size", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_IBRD_OFFSET (TotalBwOffset),           "TotalBw Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_IBRD_OFFSET (IOMissBwOffset),          "IO Miss Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_IBRD_OFFSET (TotalBwClump),            "TotalBw Clump", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_IBRD_OFFSET (IOMissBwClump),           "IO Miss Clump", 0},
    {ACPI_DMT_UINT56,   ACPI_ERDT_IBRD_OFFSET (Reserved3),               "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_IBRD_OFFSET (CounterWidth),            "Counter Width", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_IBRD_OFFSET (UpScale),                 "Upscale factor", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_IBRD_OFFSET (CorrFactorListLen),       "Corr Factor List Length", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtIbrdCorrFactor[] =
{
    {ACPI_DMT_UINT32,   0,                                               "CorrFactor", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - O bandwidth Allocation Registers for Device Agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtIbad[] =
{
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RMDD - Cache Allocation Registers for Device Agents subtable
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErdtCard[] =
{
    {ACPI_DMT_UINT32,   ACPI_ERDT_CARD_OFFSET (Reserved1),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_CARD_OFFSET (Flags),                   "Flags", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_CARD_OFFSET (ContentionMask),          "ContentionMask", 0},
    {ACPI_DMT_UINT8,    ACPI_ERDT_CARD_OFFSET (IndexFn),                 "Register Index Function", 0},
    {ACPI_DMT_UINT56,   ACPI_ERDT_CARD_OFFSET (Reserved2),               "Register Index Function", 0},
    {ACPI_DMT_UINT64,   ACPI_ERDT_CARD_OFFSET (RegBase),                 "CARD Register Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_ERDT_CARD_OFFSET (RegSize),                 "CARD Register Size", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_CARD_OFFSET (CatRegOffset),            "CARD Register Offset", 0},
    {ACPI_DMT_UINT16,   ACPI_ERDT_CARD_OFFSET (CatRegBlockSize),         "CARD Register Block Size", 0},

    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * ERST - Error Record Serialization table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoErst[] =
{
    {ACPI_DMT_UINT32,   ACPI_ERST_OFFSET (HeaderLength),            "Serialization Header Length", 0},
    {ACPI_DMT_UINT32,   ACPI_ERST_OFFSET (Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_ERST_OFFSET (Entries),                 "Instruction Entry Count", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoErst0[] =
{
    {ACPI_DMT_ERSTACT,  ACPI_ERST0_OFFSET (Action),                 "Action", 0},
    {ACPI_DMT_ERSTINST, ACPI_ERST0_OFFSET (Instruction),            "Instruction", 0},
    {ACPI_DMT_UINT8,    ACPI_ERST0_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_ERST0_FLAG_OFFSET (Flags,0),           "Preserve Register Bits", 0},

    {ACPI_DMT_UINT8,    ACPI_ERST0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_GAS,      ACPI_ERST0_OFFSET (RegisterRegion),         "Register Region", 0},
    {ACPI_DMT_UINT64,   ACPI_ERST0_OFFSET (Value),                  "Value", 0},
    {ACPI_DMT_UINT64,   ACPI_ERST0_OFFSET (Mask),                   "Mask", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * FPDT - Firmware Performance Data Table (ACPI 5.0)
 *
 ******************************************************************************/

/* Main table consists of only the standard ACPI header - subtables follow */

/* FPDT subtable header */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFpdtHdr[] =
{
    {ACPI_DMT_UINT16,   ACPI_FPDTH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT8,    ACPI_FPDTH_OFFSET (Length),                 "Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_FPDTH_OFFSET (Revision),               "Revision", 0},
    ACPI_DMT_TERMINATOR
};

/* 0: Firmware Basic Boot Performance Record */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFpdt0[] =
{
    {ACPI_DMT_UINT32,   ACPI_FPDT0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_FPDT1_OFFSET (Address),                "FPDT Boot Record Address", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: S3 Performance Table Pointer Record */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFpdt1[] =
{
    {ACPI_DMT_UINT32,   ACPI_FPDT1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_FPDT1_OFFSET (Address),                "S3PT Record Address", 0},
    ACPI_DMT_TERMINATOR
};

#if 0
    /* Boot Performance Record, not supported at this time. */
    {ACPI_DMT_UINT64,   ACPI_FPDT0_OFFSET (ResetEnd),               "Reset End", 0},
    {ACPI_DMT_UINT64,   ACPI_FPDT0_OFFSET (LoadStart),              "Load Image Start", 0},
    {ACPI_DMT_UINT64,   ACPI_FPDT0_OFFSET (StartupStart),           "Start Image Start", 0},
    {ACPI_DMT_UINT64,   ACPI_FPDT0_OFFSET (ExitServicesEntry),      "Exit Services Entry", 0},
    {ACPI_DMT_UINT64,   ACPI_FPDT0_OFFSET (ExitServicesExit),       "Exit Services Exit", 0},
#endif


/*******************************************************************************
 *
 * GTDT - Generic Timer Description Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoGtdt[] =
{
    {ACPI_DMT_UINT64,   ACPI_GTDT_OFFSET (CounterBlockAddresss),    "Counter Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_NEW_LINE,
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (SecureEl1Interrupt),      "Secure EL1 Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (SecureEl1Flags),          "EL1 Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_GTDT_FLAG_OFFSET (SecureEl1Flags,0),   "Trigger Mode", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT_FLAG_OFFSET (SecureEl1Flags,0),   "Polarity", 0},
    {ACPI_DMT_FLAG2,    ACPI_GTDT_FLAG_OFFSET (SecureEl1Flags,0),   "Always On", 0},
    ACPI_DMT_NEW_LINE,
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (NonSecureEl1Interrupt),   "Non-Secure EL1 Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (NonSecureEl1Flags),       "NEL1 Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_GTDT_FLAG_OFFSET (NonSecureEl1Flags,0),"Trigger Mode", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT_FLAG_OFFSET (NonSecureEl1Flags,0),"Polarity", 0},
    {ACPI_DMT_FLAG2,    ACPI_GTDT_FLAG_OFFSET (NonSecureEl1Flags,0),"Always On", 0},
    ACPI_DMT_NEW_LINE,
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (VirtualTimerInterrupt),   "Virtual Timer Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (VirtualTimerFlags),       "VT Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_GTDT_FLAG_OFFSET (VirtualTimerFlags,0),"Trigger Mode", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT_FLAG_OFFSET (VirtualTimerFlags,0),"Polarity", 0},
    {ACPI_DMT_FLAG2,    ACPI_GTDT_FLAG_OFFSET (VirtualTimerFlags,0),"Always On", 0},
    ACPI_DMT_NEW_LINE,
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (NonSecureEl2Interrupt),   "Non-Secure EL2 Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (NonSecureEl2Flags),       "NEL2 Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_GTDT_FLAG_OFFSET (NonSecureEl2Flags,0),"Trigger Mode", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT_FLAG_OFFSET (NonSecureEl2Flags,0),"Polarity", 0},
    {ACPI_DMT_FLAG2,    ACPI_GTDT_FLAG_OFFSET (NonSecureEl2Flags,0),"Always On", 0},
    {ACPI_DMT_UINT64,   ACPI_GTDT_OFFSET (CounterReadBlockAddress), "Counter Read Block Address", 0},
    ACPI_DMT_NEW_LINE,
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (PlatformTimerCount),      "Platform Timer Count", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT_OFFSET (PlatformTimerOffset),     "Platform Timer Offset", 0},
    ACPI_DMT_TERMINATOR
};

/* GDTD EL2 timer info. This table is appended to AcpiDmTableInfoGtdt for rev 3 and later */

ACPI_DMTABLE_INFO AcpiDmTableInfoGtdtEl2[] =
{
    {ACPI_DMT_UINT32,   ACPI_GTDT_EL2_OFFSET (VirtualEL2TimerGsiv),  "Virtual EL2 Timer GSIV", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT_EL2_OFFSET (VirtualEL2TimerFlags), "Virtual EL2 Timer Flags", 0},
    ACPI_DMT_TERMINATOR
};

/* GTDT Subtable header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoGtdtHdr[] =
{
    {ACPI_DMT_GTDT,     ACPI_GTDTH_OFFSET (Type),                   "Subtable Type", 0},
    {ACPI_DMT_UINT16,   ACPI_GTDTH_OFFSET (Length),                 "Length", DT_LENGTH},
    ACPI_DMT_TERMINATOR
};

/* GTDT Subtables */

ACPI_DMTABLE_INFO           AcpiDmTableInfoGtdt0[] =
{
    {ACPI_DMT_UINT8,    ACPI_GTDT0_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_GTDT0_OFFSET (BlockAddress),           "Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT0_OFFSET (TimerCount),             "Timer Count", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT0_OFFSET (TimerOffset),            "Timer Offset", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoGtdt0a[] =
{
    {ACPI_DMT_UINT8 ,   ACPI_GTDT0a_OFFSET (FrameNumber),               "Frame Number", 0},
    {ACPI_DMT_UINT24,   ACPI_GTDT0a_OFFSET (Reserved[0]),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_GTDT0a_OFFSET (BaseAddress),               "Base Address", 0},
    {ACPI_DMT_UINT64,   ACPI_GTDT0a_OFFSET (El0BaseAddress),            "EL0 Base Address", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT0a_OFFSET (TimerInterrupt),            "Timer Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT0a_OFFSET (TimerFlags),                "Timer Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_GTDT0a_FLAG_OFFSET (TimerFlags,0),         "Trigger Mode", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT0a_FLAG_OFFSET (TimerFlags,0),         "Polarity", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT0a_OFFSET (VirtualTimerInterrupt),     "Virtual Timer Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT0a_OFFSET (VirtualTimerFlags),         "Virtual Timer Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_GTDT0a_FLAG_OFFSET (VirtualTimerFlags,0),  "Trigger Mode", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT0a_FLAG_OFFSET (VirtualTimerFlags,0),  "Polarity", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT0a_OFFSET (CommonFlags),               "Common Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_GTDT0a_FLAG_OFFSET (CommonFlags,0),        "Secure", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT0a_FLAG_OFFSET (CommonFlags,0),        "Always On", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoGtdt1[] =
{
    {ACPI_DMT_UINT8,    ACPI_GTDT1_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT64,   ACPI_GTDT1_OFFSET (RefreshFrameAddress),    "Refresh Frame Address", 0},
    {ACPI_DMT_UINT64,   ACPI_GTDT1_OFFSET (ControlFrameAddress),    "Control Frame Address", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT1_OFFSET (TimerInterrupt),         "Timer Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_GTDT1_OFFSET (TimerFlags),             "Timer Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_GTDT1_FLAG_OFFSET (TimerFlags,0),      "Trigger Mode", 0},
    {ACPI_DMT_FLAG1,    ACPI_GTDT1_FLAG_OFFSET (TimerFlags,0),      "Polarity", 0},
    {ACPI_DMT_FLAG2,    ACPI_GTDT1_FLAG_OFFSET (TimerFlags,0),      "Security", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * HEST - Hardware Error Source table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest[] =
{
    {ACPI_DMT_UINT32,   ACPI_HEST_OFFSET (ErrorSourceCount),        "Error Source Count", 0},
    ACPI_DMT_TERMINATOR
};

/* Common HEST structures for subtables */

#define ACPI_DM_HEST_HEADER \
    {ACPI_DMT_HEST,     ACPI_HEST0_OFFSET (Header.Type),            "Subtable Type", 0}, \
    {ACPI_DMT_UINT16,   ACPI_HEST0_OFFSET (Header.SourceId),        "Source Id", 0}

#define ACPI_DM_HEST_AER \
    {ACPI_DMT_UINT16,   ACPI_HEST6_OFFSET (Aer.Reserved1),              "Reserved", 0}, \
    {ACPI_DMT_UINT8,    ACPI_HEST6_OFFSET (Aer.Flags),                  "Flags (decoded below)", DT_FLAG}, \
    {ACPI_DMT_FLAG0,    ACPI_HEST6_FLAG_OFFSET (Aer.Flags,0),           "Firmware First", 0}, \
    {ACPI_DMT_FLAG0,    ACPI_HEST6_FLAG_OFFSET (Aer.Flags,0),           "Global", 0}, \
    {ACPI_DMT_UINT8,    ACPI_HEST6_OFFSET (Aer.Enabled),                "Enabled", 0}, \
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (Aer.RecordsToPreallocate),   "Records To Preallocate", 0}, \
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (Aer.MaxSectionsPerRecord),   "Max Sections Per Record", 0}, \
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (Aer.Bus),                    "Bus", 0}, \
    {ACPI_DMT_UINT16,   ACPI_HEST6_OFFSET (Aer.Device),                 "Device", 0}, \
    {ACPI_DMT_UINT16,   ACPI_HEST6_OFFSET (Aer.Function),               "Function", 0}, \
    {ACPI_DMT_UINT16,   ACPI_HEST6_OFFSET (Aer.DeviceControl),          "DeviceControl", 0}, \
    {ACPI_DMT_UINT16,   ACPI_HEST6_OFFSET (Aer.Reserved2),              "Reserved", 0}, \
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (Aer.UncorrectableMask),      "Uncorrectable Mask", 0}, \
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (Aer.UncorrectableSeverity),  "Uncorrectable Severity", 0}, \
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (Aer.CorrectableMask),        "Correctable Mask", 0}, \
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (Aer.AdvancedCapabilities),   "Advanced Capabilities", 0}


/* HEST Subtables */

/* 0: IA32 Machine Check Exception */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest0[] =
{
    ACPI_DM_HEST_HEADER,
    {ACPI_DMT_UINT16,   ACPI_HEST0_OFFSET (Reserved1),              "Reserved1", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST0_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_HEST0_FLAG_OFFSET (Flags,0),           "Firmware First", 0},
    {ACPI_DMT_FLAG2,    ACPI_HEST0_FLAG_OFFSET (Flags,0),           "GHES Assist", 0},

    {ACPI_DMT_UINT8,    ACPI_HEST0_OFFSET (Enabled),                "Enabled", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST0_OFFSET (RecordsToPreallocate),   "Records To Preallocate", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST0_OFFSET (MaxSectionsPerRecord),   "Max Sections Per Record", 0},
    {ACPI_DMT_UINT64,   ACPI_HEST0_OFFSET (GlobalCapabilityData),   "Global Capability Data", 0},
    {ACPI_DMT_UINT64,   ACPI_HEST0_OFFSET (GlobalControlData),      "Global Control Data", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST0_OFFSET (NumHardwareBanks),       "Num Hardware Banks", 0},
    {ACPI_DMT_UINT56,   ACPI_HEST0_OFFSET (Reserved3[0]),           "Reserved2", 0},
    ACPI_DMT_TERMINATOR
};

/* 1: IA32 Corrected Machine Check */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest1[] =
{
    ACPI_DM_HEST_HEADER,
    {ACPI_DMT_UINT16,   ACPI_HEST1_OFFSET (Reserved1),              "Reserved1", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST1_OFFSET (Flags),                  "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_HEST1_FLAG_OFFSET (Flags,0),           "Firmware First", 0},
    {ACPI_DMT_FLAG2,    ACPI_HEST1_FLAG_OFFSET (Flags,0),           "GHES Assist", 0},

    {ACPI_DMT_UINT8,    ACPI_HEST1_OFFSET (Enabled),                "Enabled", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST1_OFFSET (RecordsToPreallocate),   "Records To Preallocate", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST1_OFFSET (MaxSectionsPerRecord),   "Max Sections Per Record", 0},
    {ACPI_DMT_HESTNTFY, ACPI_HEST1_OFFSET (Notify),                 "Notify", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST1_OFFSET (NumHardwareBanks),       "Num Hardware Banks", 0},
    {ACPI_DMT_UINT24,   ACPI_HEST1_OFFSET (Reserved2[0]),           "Reserved2", 0},
    ACPI_DMT_TERMINATOR
};

/* 2: IA32 Non-Maskable Interrupt */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest2[] =
{
    ACPI_DM_HEST_HEADER,
    {ACPI_DMT_UINT32,   ACPI_HEST2_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST2_OFFSET (RecordsToPreallocate),   "Records To Preallocate", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST2_OFFSET (MaxSectionsPerRecord),   "Max Sections Per Record", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST2_OFFSET (MaxRawDataLength),       "Max Raw Data Length", 0},
    ACPI_DMT_TERMINATOR
};

/* 6: PCI Express Root Port AER */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest6[] =
{
    ACPI_DM_HEST_HEADER,
    ACPI_DM_HEST_AER,
    {ACPI_DMT_UINT32,   ACPI_HEST6_OFFSET (RootErrorCommand),       "Root Error Command", 0},
    ACPI_DMT_TERMINATOR
};

/* 7: PCI Express AER (AER Endpoint) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest7[] =
{
    ACPI_DM_HEST_HEADER,
    ACPI_DM_HEST_AER,
    ACPI_DMT_TERMINATOR
};

/* 8: PCI Express/PCI-X Bridge AER */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest8[] =
{
    ACPI_DM_HEST_HEADER,
    ACPI_DM_HEST_AER,
    {ACPI_DMT_UINT32,   ACPI_HEST8_OFFSET (UncorrectableMask2),     "2nd Uncorrectable Mask", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST8_OFFSET (UncorrectableSeverity2), "2nd Uncorrectable Severity", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST8_OFFSET (AdvancedCapabilities2),  "2nd Advanced Capabilities", 0},
    ACPI_DMT_TERMINATOR
};

/* 9: Generic Hardware Error Source */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest9[] =
{
    ACPI_DM_HEST_HEADER,
    {ACPI_DMT_UINT16,   ACPI_HEST9_OFFSET (RelatedSourceId),        "Related Source Id", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST9_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST9_OFFSET (Enabled),                "Enabled", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST9_OFFSET (RecordsToPreallocate),   "Records To Preallocate", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST9_OFFSET (MaxSectionsPerRecord),   "Max Sections Per Record", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST9_OFFSET (MaxRawDataLength),       "Max Raw Data Length", 0},
    {ACPI_DMT_GAS,      ACPI_HEST9_OFFSET (ErrorStatusAddress),     "Error Status Address", 0},
    {ACPI_DMT_HESTNTFY, ACPI_HEST9_OFFSET (Notify),                 "Notify", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST9_OFFSET (ErrorBlockLength),       "Error Status Block Length", 0},
    ACPI_DMT_TERMINATOR
};

/* 10: Generic Hardware Error Source - Version 2 */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest10[] =
{
    ACPI_DM_HEST_HEADER,
    {ACPI_DMT_UINT16,   ACPI_HEST10_OFFSET (RelatedSourceId),       "Related Source Id", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST10_OFFSET (Reserved),              "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST10_OFFSET (Enabled),               "Enabled", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST10_OFFSET (RecordsToPreallocate),  "Records To Preallocate", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST10_OFFSET (MaxSectionsPerRecord),  "Max Sections Per Record", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST10_OFFSET (MaxRawDataLength),      "Max Raw Data Length", 0},
    {ACPI_DMT_GAS,      ACPI_HEST10_OFFSET (ErrorStatusAddress),    "Error Status Address", 0},
    {ACPI_DMT_HESTNTFY, ACPI_HEST10_OFFSET (Notify),                "Notify", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST10_OFFSET (ErrorBlockLength),      "Error Status Block Length", 0},
    {ACPI_DMT_GAS,      ACPI_HEST10_OFFSET (ReadAckRegister),       "Read Ack Register", 0},
    {ACPI_DMT_UINT64,   ACPI_HEST10_OFFSET (ReadAckPreserve),       "Read Ack Preserve", 0},
    {ACPI_DMT_UINT64,   ACPI_HEST10_OFFSET (ReadAckWrite),          "Read Ack Write", 0},
    ACPI_DMT_TERMINATOR
};

/* 11: IA32 Deferred Machine Check */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHest11[] =
{
    ACPI_DM_HEST_HEADER,
    {ACPI_DMT_UINT16,   ACPI_HEST11_OFFSET (Reserved1),             "Reserved1", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST11_OFFSET (Flags),                 "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_HEST11_FLAG_OFFSET (Flags,0),          "Firmware First", 0},
    {ACPI_DMT_FLAG2,    ACPI_HEST11_FLAG_OFFSET (Flags,0),          "GHES Assist", 0},

    {ACPI_DMT_UINT8,    ACPI_HEST11_OFFSET (Enabled),               "Enabled", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST11_OFFSET (RecordsToPreallocate),  "Records To Preallocate", 0},
    {ACPI_DMT_UINT32,   ACPI_HEST11_OFFSET (MaxSectionsPerRecord),  "Max Sections Per Record", 0},
    {ACPI_DMT_HESTNTFY, ACPI_HEST11_OFFSET (Notify),                "Notify", 0},
    {ACPI_DMT_UINT8,    ACPI_HEST11_OFFSET (NumHardwareBanks),      "Num Hardware Banks", 0},
    {ACPI_DMT_UINT24,   ACPI_HEST11_OFFSET (Reserved2[0]),          "Reserved2", 0},
    ACPI_DMT_TERMINATOR
};

/* Notification Structure */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHestNotify[] =
{
    {ACPI_DMT_HESTNTYP, ACPI_HESTN_OFFSET (Type),                   "Notify Type", 0},
    {ACPI_DMT_UINT8,    ACPI_HESTN_OFFSET (Length),                 "Notify Length", DT_LENGTH},
    {ACPI_DMT_UINT16,   ACPI_HESTN_OFFSET (ConfigWriteEnable),      "Configuration Write Enable", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTN_OFFSET (PollInterval),           "PollInterval", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTN_OFFSET (Vector),                 "Vector", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTN_OFFSET (PollingThresholdValue),  "Polling Threshold Value", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTN_OFFSET (PollingThresholdWindow), "Polling Threshold Window", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTN_OFFSET (ErrorThresholdValue),    "Error Threshold Value", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTN_OFFSET (ErrorThresholdWindow),   "Error Threshold Window", 0},
    ACPI_DMT_TERMINATOR
};


/*
 * IA32 Error Bank(s) - Follows the ACPI_HEST_IA_MACHINE_CHECK and
 * ACPI_HEST_IA_CORRECTED structures.
 */
ACPI_DMTABLE_INFO           AcpiDmTableInfoHestBank[] =
{
    {ACPI_DMT_UINT8,    ACPI_HESTB_OFFSET (BankNumber),             "Bank Number", 0},
    {ACPI_DMT_UINT8,    ACPI_HESTB_OFFSET (ClearStatusOnInit),      "Clear Status On Init", 0},
    {ACPI_DMT_UINT8,    ACPI_HESTB_OFFSET (StatusFormat),           "Status Format", 0},
    {ACPI_DMT_UINT8,    ACPI_HESTB_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTB_OFFSET (ControlRegister),        "Control Register", 0},
    {ACPI_DMT_UINT64,   ACPI_HESTB_OFFSET (ControlData),            "Control Data", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTB_OFFSET (StatusRegister),         "Status Register", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTB_OFFSET (AddressRegister),        "Address Register", 0},
    {ACPI_DMT_UINT32,   ACPI_HESTB_OFFSET (MiscRegister),           "Misc Register", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * HMAT - Heterogeneous Memory Attributes Table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat[] =
{
    {ACPI_DMT_UINT32,   ACPI_HMAT_OFFSET (Reserved),                "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* Common HMAT structure header (one per Subtable) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmatHdr[] =
{
    {ACPI_DMT_HMAT,     ACPI_HMATH_OFFSET (Type),                   "Structure Type", 0},
    {ACPI_DMT_UINT16,   ACPI_HMATH_OFFSET (Reserved),               "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_HMATH_OFFSET (Length),                 "Length", 0},
    ACPI_DMT_TERMINATOR
};

/* HMAT subtables */

/* 0x00: Memory proximity domain attributes */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat0[] =
{
    {ACPI_DMT_UINT16,   ACPI_HMAT0_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAG0,    ACPI_HMAT0_FLAG_OFFSET (Flags,0),           "Processor Proximity Domain Valid", 0},
    {ACPI_DMT_UINT16,   ACPI_HMAT0_OFFSET (Reserved1),              "Reserved1", 0},
    {ACPI_DMT_UINT32,   ACPI_HMAT0_OFFSET (InitiatorPD),            "Attached Initiator Proximity Domain", 0},
    {ACPI_DMT_UINT32,   ACPI_HMAT0_OFFSET (MemoryPD),               "Memory Proximity Domain", 0},
    {ACPI_DMT_UINT32,   ACPI_HMAT0_OFFSET (Reserved2),              "Reserved2", 0},
    {ACPI_DMT_UINT64,   ACPI_HMAT0_OFFSET (Reserved3),              "Reserved3", 0},
    {ACPI_DMT_UINT64,   ACPI_HMAT0_OFFSET (Reserved4),              "Reserved4", 0},
    ACPI_DMT_TERMINATOR
};

/* 0x01: System Locality Latency and Bandwidth Information */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat1[] =
{
    {ACPI_DMT_UINT8,    ACPI_HMAT1_OFFSET (Flags),                  "Flags (decoded below)", 0},
    {ACPI_DMT_FLAGS4_0, ACPI_HMAT1_FLAG_OFFSET (Flags,0),           "Memory Hierarchy", 0},         /* First 4 bits */
    {ACPI_DMT_FLAG4,    ACPI_HMAT1_FLAG_OFFSET (Flags,0),           "Use Minimum Transfer Size", 0},
    {ACPI_DMT_FLAG5,    ACPI_HMAT1_FLAG_OFFSET (Flags,0),           "Non-sequential Transfers", 0},
    {ACPI_DMT_UINT8,    ACPI_HMAT1_OFFSET (DataType),               "Data Type", 0},
    {ACPI_DMT_UINT8,    ACPI_HMAT1_OFFSET (MinTransferSize),        "Minimum Transfer Size", 0},
    {ACPI_DMT_UINT8,    ACPI_HMAT1_OFFSET (Reserved1),              "Reserved1", 0},
    {ACPI_DMT_UINT32,   ACPI_HMAT1_OFFSET (NumberOfInitiatorPDs),   "Initiator Proximity Domains #", 0},
    {ACPI_DMT_UINT32,   ACPI_HMAT1_OFFSET (NumberOfTargetPDs),      "Target Proximity Domains #", 0},
    {ACPI_DMT_UINT32,   ACPI_HMAT1_OFFSET (Reserved2),              "Reserved2", 0},
    {ACPI_DMT_UINT64,   ACPI_HMAT1_OFFSET (EntryBaseUnit),          "Entry Base Unit", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat1a[] =
{
    {ACPI_DMT_UINT32,   0,                                          "Initiator Proximity Domain List", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat1b[] =
{
    {ACPI_DMT_UINT32,   0,                                          "Target Proximity Domain List", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat1c[] =
{
    {ACPI_DMT_UINT16,   0,                                          "Entry", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};

/* 0x02: Memory Side Cache Information */

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat2[] =
{
    {ACPI_DMT_UINT32,       ACPI_HMAT2_OFFSET (MemoryPD),               "Memory Proximity Domain", 0},
    {ACPI_DMT_UINT32,       ACPI_HMAT2_OFFSET (Reserved1),              "Reserved1", 0},
    {ACPI_DMT_UINT64,       ACPI_HMAT2_OFFSET (CacheSize),              "Memory Side Cache Size", 0},
    {ACPI_DMT_UINT32,       ACPI_HMAT2_OFFSET (CacheAttributes),        "Cache Attributes (decoded below)", 0},
    {ACPI_DMT_FLAGS4_0,     ACPI_HMAT2_FLAG_OFFSET (CacheAttributes,0), "Total Cache Levels", 0},
    {ACPI_DMT_FLAGS4_4,     ACPI_HMAT2_FLAG_OFFSET (CacheAttributes,0), "Cache Level", 0},
    {ACPI_DMT_FLAGS4_8,     ACPI_HMAT2_FLAG_OFFSET (CacheAttributes,0), "Cache Associativity", 0},
    {ACPI_DMT_FLAGS4_12,    ACPI_HMAT2_FLAG_OFFSET (CacheAttributes,0), "Write Policy", 0},
    {ACPI_DMT_FLAGS16_16,   ACPI_HMAT2_FLAG_OFFSET (CacheAttributes,0), "Cache Line Size", 0},
    {ACPI_DMT_UINT16,       ACPI_HMAT2_OFFSET (AddressMode),            "Address Mode", 0},
    {ACPI_DMT_UINT16,       ACPI_HMAT2_OFFSET (NumberOfSMBIOSHandles),  "SMBIOS Handle #", 0},
    ACPI_DMT_TERMINATOR
};

ACPI_DMTABLE_INFO           AcpiDmTableInfoHmat2a[] =
{
    {ACPI_DMT_UINT16,   0,                                          "SMBIOS Handle", DT_OPTIONAL},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * HPET - High Precision Event Timer table
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoHpet[] =
{
    {ACPI_DMT_UINT32,   ACPI_HPET_OFFSET (Id),                      "Hardware Block ID", 0},
    {ACPI_DMT_GAS,      ACPI_HPET_OFFSET (Address),                 "Timer Block Register", 0},
    {ACPI_DMT_UINT8,    ACPI_HPET_OFFSET (Sequence),                "Sequence Number", 0},
    {ACPI_DMT_UINT16,   ACPI_HPET_OFFSET (MinimumTick),             "Minimum Clock Ticks", 0},
    {ACPI_DMT_UINT8,    ACPI_HPET_OFFSET (Flags),                   "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_HPET_FLAG_OFFSET (Flags,0),            "4K Page Protect", 0},
    {ACPI_DMT_FLAG1,    ACPI_HPET_FLAG_OFFSET (Flags,0),            "64K Page Protect", 0},
    ACPI_DMT_TERMINATOR
};
/*! [End] no source code translation !*/
