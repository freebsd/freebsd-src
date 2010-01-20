/******************************************************************************
 *
 * Name: actbl1.h - Additional ACPI table definitions
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

#ifndef __ACTBL1_H__
#define __ACTBL1_H__


/*******************************************************************************
 *
 * Additional ACPI Tables
 *
 * These tables are not consumed directly by the ACPICA subsystem, but are
 * included here to support device drivers and the AML disassembler.
 *
 ******************************************************************************/


/*
 * Values for description table header signatures. Useful because they make
 * it more difficult to inadvertently type in the wrong signature.
 */
#define ACPI_SIG_ASF            "ASF!"      /* Alert Standard Format table */
#define ACPI_SIG_BERT           "BERT"      /* Boot Error Record Table */
#define ACPI_SIG_BOOT           "BOOT"      /* Simple Boot Flag Table */
#define ACPI_SIG_CPEP           "CPEP"      /* Corrected Platform Error Polling table */
#define ACPI_SIG_DBGP           "DBGP"      /* Debug Port table */
#define ACPI_SIG_DMAR           "DMAR"      /* DMA Remapping table */
#define ACPI_SIG_ECDT           "ECDT"      /* Embedded Controller Boot Resources Table */
#define ACPI_SIG_EINJ           "EINJ"      /* Error Injection table */
#define ACPI_SIG_ERST           "ERST"      /* Error Record Serialization Table */
#define ACPI_SIG_HEST           "HEST"      /* Hardware Error Source Table */
#define ACPI_SIG_HPET           "HPET"      /* High Precision Event Timer table */
#define ACPI_SIG_IBFT           "IBFT"      /* iSCSI Boot Firmware Table */
#define ACPI_SIG_MADT           "APIC"      /* Multiple APIC Description Table */
#define ACPI_SIG_MCFG           "MCFG"      /* PCI Memory Mapped Configuration table */
#define ACPI_SIG_SBST           "SBST"      /* Smart Battery Specification Table */
#define ACPI_SIG_SLIC           "SLIC"      /* Software Licensing Description Table */
#define ACPI_SIG_SLIT           "SLIT"      /* System Locality Distance Information Table */
#define ACPI_SIG_SPCR           "SPCR"      /* Serial Port Console Redirection table */
#define ACPI_SIG_SPMI           "SPMI"      /* Server Platform Management Interface table */
#define ACPI_SIG_SRAT           "SRAT"      /* System Resource Affinity Table */
#define ACPI_SIG_TCPA           "TCPA"      /* Trusted Computing Platform Alliance table */
#define ACPI_SIG_UEFI           "UEFI"      /* Uefi Boot Optimization Table */
#define ACPI_SIG_WDAT           "WDAT"      /* Watchdog Action Table */
#define ACPI_SIG_WDRT           "WDRT"      /* Watchdog Resource Table */


/*
 * All tables must be byte-packed to match the ACPI specification, since
 * the tables are provided by the system BIOS.
 */
#pragma pack(1)

/*
 * Note about bitfields: The UINT8 type is used for bitfields in ACPI tables.
 * This is the only type that is even remotely portable. Anything else is not
 * portable, so do not use any other bitfield types.
 */


/* Common Subtable header (used in MADT, SRAT, etc.) */

typedef struct acpi_subtable_header
{
    UINT8                   Type;
    UINT8                   Length;

} ACPI_SUBTABLE_HEADER;


/* Common Subtable header for WHEA tables (EINJ, ERST, WDAT) */

typedef struct acpi_whea_header
{
    UINT8                   Action;
    UINT8                   Instruction;
    UINT8                   Flags;
    UINT8                   Reserved;
    ACPI_GENERIC_ADDRESS    RegisterRegion;
    UINT64                  Value;              /* Value used with Read/Write register */
    UINT64                  Mask;               /* Bitmask required for this register instruction */

} ACPI_WHEA_HEADER;


/*******************************************************************************
 *
 * ASF - Alert Standard Format table (Signature "ASF!")
 *
 * Conforms to the Alert Standard Format Specification V2.0, 23 April 2003
 *
 ******************************************************************************/

typedef struct acpi_table_asf
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */

} ACPI_TABLE_ASF;


/* ASF subtable header */

typedef struct acpi_asf_header
{
    UINT8                   Type;
    UINT8                   Reserved;
    UINT16                  Length;

} ACPI_ASF_HEADER;


/* Values for Type field above */

enum AcpiAsfType
{
    ACPI_ASF_TYPE_INFO          = 0,
    ACPI_ASF_TYPE_ALERT         = 1,
    ACPI_ASF_TYPE_CONTROL       = 2,
    ACPI_ASF_TYPE_BOOT          = 3,
    ACPI_ASF_TYPE_ADDRESS       = 4,
    ACPI_ASF_TYPE_RESERVED      = 5
};

/*
 * ASF subtables
 */

/* 0: ASF Information */

typedef struct acpi_asf_info
{
    ACPI_ASF_HEADER         Header;
    UINT8                   MinResetValue;
    UINT8                   MinPollInterval;
    UINT16                  SystemId;
    UINT32                  MfgId;
    UINT8                   Flags;
    UINT8                   Reserved2[3];

} ACPI_ASF_INFO;

/* 1: ASF Alerts */

typedef struct acpi_asf_alert
{
    ACPI_ASF_HEADER         Header;
    UINT8                   AssertMask;
    UINT8                   DeassertMask;
    UINT8                   Alerts;
    UINT8                   DataLength;

} ACPI_ASF_ALERT;

typedef struct acpi_asf_alert_data
{
    UINT8                   Address;
    UINT8                   Command;
    UINT8                   Mask;
    UINT8                   Value;
    UINT8                   SensorType;
    UINT8                   Type;
    UINT8                   Offset;
    UINT8                   SourceType;
    UINT8                   Severity;
    UINT8                   SensorNumber;
    UINT8                   Entity;
    UINT8                   Instance;

} ACPI_ASF_ALERT_DATA;

/* 2: ASF Remote Control */

typedef struct acpi_asf_remote
{
    ACPI_ASF_HEADER         Header;
    UINT8                   Controls;
    UINT8                   DataLength;
    UINT16                  Reserved2;

} ACPI_ASF_REMOTE;

typedef struct acpi_asf_control_data
{
    UINT8                   Function;
    UINT8                   Address;
    UINT8                   Command;
    UINT8                   Value;

} ACPI_ASF_CONTROL_DATA;

/* 3: ASF RMCP Boot Options */

typedef struct acpi_asf_rmcp
{
    ACPI_ASF_HEADER         Header;
    UINT8                   Capabilities[7];
    UINT8                   CompletionCode;
    UINT32                  EnterpriseId;
    UINT8                   Command;
    UINT16                  Parameter;
    UINT16                  BootOptions;
    UINT16                  OemParameters;

} ACPI_ASF_RMCP;

/* 4: ASF Address */

typedef struct acpi_asf_address
{
    ACPI_ASF_HEADER         Header;
    UINT8                   EpromAddress;
    UINT8                   Devices;

} ACPI_ASF_ADDRESS;


/*******************************************************************************
 *
 * BERT - Boot Error Record Table
 *
 ******************************************************************************/

typedef struct acpi_table_bert
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  RegionLength;       /* Length of the boot error region */
    UINT64                  Address;            /* Physical addresss of the error region */

} ACPI_TABLE_BERT;


/* Boot Error Region */

typedef struct acpi_bert_region
{
    UINT32                  BlockStatus;
    UINT32                  RawDataOffset;
    UINT32                  RawDataLength;
    UINT32                  DataLength;
    UINT32                  ErrorSeverity;

} ACPI_BERT_REGION;

/* BlockStatus Flags */

#define ACPI_BERT_UNCORRECTABLE             (1)
#define ACPI_BERT_CORRECTABLE               (2)
#define ACPI_BERT_MULTIPLE_UNCORRECTABLE    (4)
#define ACPI_BERT_MULTIPLE_CORRECTABLE      (8)


/*******************************************************************************
 *
 * BOOT - Simple Boot Flag Table
 *
 ******************************************************************************/

typedef struct acpi_table_boot
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   CmosIndex;          /* Index in CMOS RAM for the boot register */
    UINT8                   Reserved[3];

} ACPI_TABLE_BOOT;


/*******************************************************************************
 *
 * CPEP - Corrected Platform Error Polling table
 *
 ******************************************************************************/

typedef struct acpi_table_cpep
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT64                  Reserved;

} ACPI_TABLE_CPEP;


/* Subtable */

typedef struct acpi_cpep_polling
{
    UINT8                   Type;
    UINT8                   Length;
    UINT8                   Id;                 /* Processor ID */
    UINT8                   Eid;                /* Processor EID */
    UINT32                  Interval;           /* Polling interval (msec) */

} ACPI_CPEP_POLLING;


/*******************************************************************************
 *
 * DBGP - Debug Port table
 *
 ******************************************************************************/

typedef struct acpi_table_dbgp
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   Type;               /* 0=full 16550, 1=subset of 16550 */
    UINT8                   Reserved[3];
    ACPI_GENERIC_ADDRESS    DebugPort;

} ACPI_TABLE_DBGP;


/*******************************************************************************
 *
 * DMAR - DMA Remapping table
 *        From "Intel Virtualization Technology for Directed I/O", Sept. 2007
 *
 ******************************************************************************/

typedef struct acpi_table_dmar
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   Width;              /* Host Address Width */
    UINT8                   Flags;
    UINT8                   Reserved[10];

} ACPI_TABLE_DMAR;

/* Flags */

#define ACPI_DMAR_INTR_REMAP        (1)

/* DMAR subtable header */

typedef struct acpi_dmar_header
{
    UINT16                  Type;
    UINT16                  Length;

} ACPI_DMAR_HEADER;

/* Values for subtable type in ACPI_DMAR_HEADER */

enum AcpiDmarType
{
    ACPI_DMAR_TYPE_HARDWARE_UNIT        = 0,
    ACPI_DMAR_TYPE_RESERVED_MEMORY      = 1,
    ACPI_DMAR_TYPE_ATSR                 = 2,
    ACPI_DMAR_TYPE_RESERVED             = 3     /* 3 and greater are reserved */
};

typedef struct acpi_dmar_device_scope
{
    UINT8                   EntryType;
    UINT8                   Length;
    UINT16                  Reserved;
    UINT8                   EnumerationId;
    UINT8                   Bus;

} ACPI_DMAR_DEVICE_SCOPE;

/* Values for EntryType in ACPI_DMAR_DEVICE_SCOPE */

enum AcpiDmarScopeType
{
    ACPI_DMAR_SCOPE_TYPE_NOT_USED       = 0,
    ACPI_DMAR_SCOPE_TYPE_ENDPOINT       = 1,
    ACPI_DMAR_SCOPE_TYPE_BRIDGE         = 2,
    ACPI_DMAR_SCOPE_TYPE_IOAPIC         = 3,
    ACPI_DMAR_SCOPE_TYPE_HPET           = 4,
    ACPI_DMAR_SCOPE_TYPE_RESERVED       = 5     /* 5 and greater are reserved */
};

typedef struct acpi_dmar_pci_path
{
    UINT8                   Device;
    UINT8                   Function;

} ACPI_DMAR_PCI_PATH;

/*
 * DMAR Sub-tables, correspond to Type in ACPI_DMAR_HEADER
 */

/* 0: Hardware Unit Definition */

typedef struct acpi_dmar_hardware_unit
{
    ACPI_DMAR_HEADER        Header;
    UINT8                   Flags;
    UINT8                   Reserved;
    UINT16                  Segment;
    UINT64                  Address;            /* Register Base Address */

} ACPI_DMAR_HARDWARE_UNIT;

/* Flags */

#define ACPI_DMAR_INCLUDE_ALL       (1)

/* 1: Reserved Memory Defininition */

typedef struct acpi_dmar_reserved_memory
{
    ACPI_DMAR_HEADER        Header;
    UINT16                  Reserved;
    UINT16                  Segment;
    UINT64                  BaseAddress;        /* 4K aligned base address */
    UINT64                  EndAddress;         /* 4K aligned limit address */

} ACPI_DMAR_RESERVED_MEMORY;

/* Flags */

#define ACPI_DMAR_ALLOW_ALL         (1)

/* 2: Root Port ATS Capability Reporting Structure */

typedef struct acpi_dmar_atsr
{
    ACPI_DMAR_HEADER        Header;
    UINT8                   Flags;
    UINT8                   Reserved;
    UINT16                  Segment;

} ACPI_DMAR_ATSR;

/* Flags */

#define ACPI_DMAR_ALL_PORTS         (1)


/*******************************************************************************
 *
 * ECDT - Embedded Controller Boot Resources Table
 *
 ******************************************************************************/

typedef struct acpi_table_ecdt
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    ACPI_GENERIC_ADDRESS    Control;            /* Address of EC command/status register */
    ACPI_GENERIC_ADDRESS    Data;               /* Address of EC data register */
    UINT32                  Uid;                /* Unique ID - must be same as the EC _UID method */
    UINT8                   Gpe;                /* The GPE for the EC */
    UINT8                   Id[1];              /* Full namepath of the EC in the ACPI namespace */

} ACPI_TABLE_ECDT;


/*******************************************************************************
 *
 * EINJ - Error Injection Table
 *
 ******************************************************************************/

typedef struct acpi_table_einj
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  HeaderLength;
    UINT32                  Reserved;
    UINT32                  Entries;

} ACPI_TABLE_EINJ;

/* EINJ Injection Instruction Entries (actions) */

typedef struct acpi_einj_entry
{
    ACPI_WHEA_HEADER        WheaHeader;         /* Common header for WHEA tables */

} ACPI_EINJ_ENTRY;

/* Values for Action field above */

enum AcpiEinjActions
{
    ACPI_EINJ_BEGIN_OPERATION       = 0,
    ACPI_EINJ_GET_TRIGGER_TABLE     = 1,
    ACPI_EINJ_SET_ERROR_TYPE        = 2,
    ACPI_EINJ_GET_ERROR_TYPE        = 3,
    ACPI_EINJ_END_OPERATION         = 4,
    ACPI_EINJ_EXECUTE_OPERATION     = 5,
    ACPI_EINJ_CHECK_BUSY_STATUS     = 6,
    ACPI_EINJ_GET_COMMAND_STATUS    = 7,
    ACPI_EINJ_ACTION_RESERVED       = 8,     /* 8 and greater are reserved */
    ACPI_EINJ_TRIGGER_ERROR         = 0xFF   /* Except for this value */
};

/* Values for Instruction field above */

enum AcpiEinjInstructions
{
    ACPI_EINJ_READ_REGISTER         = 0,
    ACPI_EINJ_READ_REGISTER_VALUE   = 1,
    ACPI_EINJ_WRITE_REGISTER        = 2,
    ACPI_EINJ_WRITE_REGISTER_VALUE  = 3,
    ACPI_EINJ_NOOP                  = 4,
    ACPI_EINJ_INSTRUCTION_RESERVED  = 5     /* 5 and greater are reserved */
};

/* EINJ Trigger Error Action Table */

typedef struct acpi_einj_trigger
{
    UINT32                  HeaderSize;
    UINT32                  Revision;
    UINT32                  TableSize;
    UINT32                  EntryCount;

} ACPI_EINJ_TRIGGER;


/*******************************************************************************
 *
 * ERST - Error Record Serialization Table
 *
 ******************************************************************************/

typedef struct acpi_table_erst
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  HeaderLength;
    UINT32                  Reserved;
    UINT32                  Entries;

} ACPI_TABLE_ERST;

/* ERST Serialization Entries (actions) */

typedef struct acpi_erst_entry
{
    ACPI_WHEA_HEADER        WheaHeader;         /* Common header for WHEA tables */

} ACPI_ERST_ENTRY;

/* Values for Action field above */

enum AcpiErstActions
{
    ACPI_ERST_BEGIN_WRITE_OPERATION = 0,
    ACPI_ERST_BEGIN_READ_OPERATION  = 1,
    ACPI_ERST_BETGIN_CLEAR_OPERATION= 2,
    ACPI_ERST_END_OPERATION         = 3,
    ACPI_ERST_SET_RECORD_OFFSET     = 4,
    ACPI_ERST_EXECUTE_OPERATION     = 5,
    ACPI_ERST_CHECK_BUSY_STATUS     = 6,
    ACPI_ERST_GET_COMMAND_STATUS    = 7,
    ACPI_ERST_GET_RECORD_IDENTIFIER = 8,
    ACPI_ERST_SET_RECORD_IDENTIFIER = 9,
    ACPI_ERST_GET_RECORD_COUNT      = 10,
    ACPI_ERST_BEGIN_DUMMY_WRIITE    = 11,
    ACPI_ERST_NOT_USED              = 12,
    ACPI_ERST_GET_ERROR_RANGE       = 13,
    ACPI_ERST_GET_ERROR_LENGTH      = 14,
    ACPI_ERST_GET_ERROR_ATTRIBUTES  = 15,
    ACPI_ERST_ACTION_RESERVED       = 16    /* 16 and greater are reserved */
};

/* Values for Instruction field above */

enum AcpiErstInstructions
{
    ACPI_ERST_READ_REGISTER         = 0,
    ACPI_ERST_READ_REGISTER_VALUE   = 1,
    ACPI_ERST_WRITE_REGISTER        = 2,
    ACPI_ERST_WRITE_REGISTER_VALUE  = 3,
    ACPI_ERST_NOOP                  = 4,
    ACPI_ERST_LOAD_VAR1             = 5,
    ACPI_ERST_LOAD_VAR2             = 6,
    ACPI_ERST_STORE_VAR1            = 7,
    ACPI_ERST_ADD                   = 8,
    ACPI_ERST_SUBTRACT              = 9,
    ACPI_ERST_ADD_VALUE             = 10,
    ACPI_ERST_SUBTRACT_VALUE        = 11,
    ACPI_ERST_STALL                 = 12,
    ACPI_ERST_STALL_WHILE_TRUE      = 13,
    ACPI_ERST_SKIP_NEXT_IF_TRUE     = 14,
    ACPI_ERST_GOTO                  = 15,
    ACPI_ERST_SET_SRC_ADDRESS_BASE  = 16,
    ACPI_ERST_SET_DST_ADDRESS_BASE  = 17,
    ACPI_ERST_MOVE_DATA             = 18,
    ACPI_ERST_INSTRUCTION_RESERVED  = 19    /* 19 and greater are reserved */
};


/*******************************************************************************
 *
 * HEST - Hardware Error Source Table
 *
 ******************************************************************************/

typedef struct acpi_table_hest
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  ErrorSourceCount;

} ACPI_TABLE_HEST;


/* HEST subtable header */

typedef struct acpi_hest_header
{
    UINT16                  Type;

} ACPI_HEST_HEADER;


/* Values for Type field above for subtables */

enum AcpiHestTypes
{
    ACPI_HEST_TYPE_XPF_MACHINE_CHECK             = 0,
    ACPI_HEST_TYPE_XPF_CORRECTED_MACHINE_CHECK   = 1,
    ACPI_HEST_TYPE_XPF_UNUSED                    = 2,
    ACPI_HEST_TYPE_XPF_NON_MASKABLE_INTERRUPT    = 3,
    ACPI_HEST_TYPE_IPF_CORRECTED_MACHINE_CHECK   = 4,
    ACPI_HEST_TYPE_IPF_CORRECTED_PLATFORM_ERROR  = 5,
    ACPI_HEST_TYPE_AER_ROOT_PORT                 = 6,
    ACPI_HEST_TYPE_AER_ENDPOINT                  = 7,
    ACPI_HEST_TYPE_AER_BRIDGE                    = 8,
    ACPI_HEST_TYPE_GENERIC_HARDWARE_ERROR_SOURCE = 9,
    ACPI_HEST_TYPE_RESERVED                      = 10    /* 10 and greater are reserved */
};


/*
 * HEST Sub-subtables
 */

/* XPF Machine Check Error Bank */

typedef struct acpi_hest_xpf_error_bank
{
    UINT8                   BankNumber;
    UINT8                   ClearStatusOnInit;
    UINT8                   StatusFormat;
    UINT8                   ConfigWriteEnable;
    UINT32                  ControlRegister;
    UINT64                  ControlInitData;
    UINT32                  StatusRegister;
    UINT32                  AddressRegister;
    UINT32                  MiscRegister;

} ACPI_HEST_XPF_ERROR_BANK;


/* Generic Error Status */

typedef struct acpi_hest_generic_status
{
    UINT32                  BlockStatus;
    UINT32                  RawDataOffset;
    UINT32                  RawDataLength;
    UINT32                  DataLength;
    UINT32                  ErrorSeverity;

} ACPI_HEST_GENERIC_STATUS;


/* Generic Error Data */

typedef struct acpi_hest_generic_data
{
    UINT8                   SectionType[16];
    UINT32                  ErrorSeverity;
    UINT16                  Revision;
    UINT8                   ValidationBits;
    UINT8                   Flags;
    UINT32                  ErrorDataLength;
    UINT8                   FruId[16];
    UINT8                   FruText[20];

} ACPI_HEST_GENERIC_DATA;


/* Common HEST structure for PCI/AER types below (6,7,8) */

typedef struct acpi_hest_aer_common
{
    UINT16                  SourceId;
    UINT16                  ConfigWriteEnable;
    UINT8                   Flags;
    UINT8                   Enabled;
    UINT32                  RecordsToPreAllocate;
    UINT32                  MaxSectionsPerRecord;
    UINT32                  Bus;
    UINT16                  Device;
    UINT16                  Function;
    UINT16                  DeviceControl;
    UINT16                  Reserved;
    UINT32                  UncorrectableErrorMask;
    UINT32                  UncorrectableErrorSeverity;
    UINT32                  CorrectableErrorMask;
    UINT32                  AdvancedErrorCapabilities;

} ACPI_HEST_AER_COMMON;


/* Hardware Error Notification */

typedef struct acpi_hest_notify
{
    UINT8                   Type;
    UINT8                   Length;
    UINT16                  ConfigWriteEnable;
    UINT32                  PollInterval;
    UINT32                  Vector;
    UINT32                  PollingThresholdValue;
    UINT32                  PollingThresholdWindow;
    UINT32                  ErrorThresholdValue;
    UINT32                  ErrorThresholdWindow;

} ACPI_HEST_NOTIFY;

/* Values for Notify Type field above */

enum AcpiHestNotifyTypes
{
    ACPI_HEST_NOTIFY_POLLED     = 0,
    ACPI_HEST_NOTIFY_EXTERNAL   = 1,
    ACPI_HEST_NOTIFY_LOCAL      = 2,
    ACPI_HEST_NOTIFY_SCI        = 3,
    ACPI_HEST_NOTIFY_NMI        = 4,
    ACPI_HEST_NOTIFY_RESERVED   = 5     /* 5 and greater are reserved */
};


/*
 * HEST subtables
 *
 * From WHEA Design Document, 16 May 2007.
 * Note: There is no subtable type 2 in this version of the document,
 * and there are two different subtable type 3s.
 */

 /* 0: XPF Machine Check Exception */

typedef struct acpi_hest_xpf_machine_check
{
    ACPI_HEST_HEADER        Header;
    UINT16                  SourceId;
    UINT16                  ConfigWriteEnable;
    UINT8                   Flags;
    UINT8                   Reserved1;
    UINT32                  RecordsToPreAllocate;
    UINT32                  MaxSectionsPerRecord;
    UINT64                  GlobalCapabilityData;
    UINT64                  GlobalControlData;
    UINT8                   NumHardwareBanks;
    UINT8                   Reserved2[7];

} ACPI_HEST_XPF_MACHINE_CHECK;


/* 1: XPF Corrected Machine Check */

typedef struct acpi_table_hest_xpf_corrected
{
    ACPI_HEST_HEADER        Header;
    UINT16                  SourceId;
    UINT16                  ConfigWriteEnable;
    UINT8                   Flags;
    UINT8                   Enabled;
    UINT32                  RecordsToPreAllocate;
    UINT32                  MaxSectionsPerRecord;
    ACPI_HEST_NOTIFY        Notify;
    UINT8                   NumHardwareBanks;
    UINT8                   Reserved[3];

} ACPI_HEST_XPF_CORRECTED;


/* 3: XPF Non-Maskable Interrupt */

typedef struct acpi_hest_xpf_nmi
{
    ACPI_HEST_HEADER        Header;
    UINT16                  SourceId;
    UINT32                  Reserved;
    UINT32                  RecordsToPreAllocate;
    UINT32                  MaxSectionsPerRecord;
    UINT32                  MaxRawDataLength;

} ACPI_HEST_XPF_NMI;


/* 4: IPF Corrected Machine Check */

typedef struct acpi_hest_ipf_corrected
{
    ACPI_HEST_HEADER        Header;
    UINT8                   Enabled;
    UINT8                   Reserved;

} ACPI_HEST_IPF_CORRECTED;


/* 5: IPF Corrected Platform Error */

typedef struct acpi_hest_ipf_corrected_platform
{
    ACPI_HEST_HEADER        Header;
    UINT8                   Enabled;
    UINT8                   Reserved;

} ACPI_HEST_IPF_CORRECTED_PLATFORM;


/* 6: PCI Express Root Port AER */

typedef struct acpi_hest_aer_root
{
    ACPI_HEST_HEADER        Header;
    ACPI_HEST_AER_COMMON    Aer;
    UINT32                  RootErrorCommand;

} ACPI_HEST_AER_ROOT;


/* 7: PCI Express AER (AER Endpoint) */

typedef struct acpi_hest_aer
{
    ACPI_HEST_HEADER        Header;
    ACPI_HEST_AER_COMMON    Aer;

} ACPI_HEST_AER;


/* 8: PCI Express/PCI-X Bridge AER */

typedef struct acpi_hest_aer_bridge
{
    ACPI_HEST_HEADER        Header;
    ACPI_HEST_AER_COMMON    Aer;
    UINT32                  SecondaryUncorrectableErrorMask;
    UINT32                  SecondaryUncorrectableErrorSeverity;
    UINT32                  SecondaryAdvancedCapabilities;

} ACPI_HEST_AER_BRIDGE;


/* 9: Generic Hardware Error Source */

typedef struct acpi_hest_generic
{
    ACPI_HEST_HEADER        Header;
    UINT16                  SourceId;
    UINT16                  RelatedSourceId;
    UINT8                   ConfigWriteEnable;
    UINT8                   Enabled;
    UINT32                  RecordsToPreAllocate;
    UINT32                  MaxSectionsPerRecord;
    UINT32                  MaxRawDataLength;
    ACPI_GENERIC_ADDRESS    ErrorStatusAddress;
    ACPI_HEST_NOTIFY        Notify;
    UINT32                  ErrorStatusBlockLength;

} ACPI_HEST_GENERIC;


/*******************************************************************************
 *
 * HPET - High Precision Event Timer table
 *
 ******************************************************************************/

typedef struct acpi_table_hpet
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  Id;                 /* Hardware ID of event timer block */
    ACPI_GENERIC_ADDRESS    Address;            /* Address of event timer block */
    UINT8                   Sequence;           /* HPET sequence number */
    UINT16                  MinimumTick;        /* Main counter min tick, periodic mode */
    UINT8                   Flags;

} ACPI_TABLE_HPET;

/*! Flags */

#define ACPI_HPET_PAGE_PROTECT      (1)         /* 00: No page protection */
#define ACPI_HPET_PAGE_PROTECT_4    (1<<1)      /* 01: 4KB page protected */
#define ACPI_HPET_PAGE_PROTECT_64   (1<<2)      /* 02: 64KB page protected */

/*! [End] no source code translation !*/


/*******************************************************************************
 *
 * IBFT - Boot Firmware Table
 *
 ******************************************************************************/

typedef struct acpi_table_ibft
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   Reserved[12];

} ACPI_TABLE_IBFT;


/* IBFT common subtable header */

typedef struct acpi_ibft_header
{
    UINT8                   Type;
    UINT8                   Version;
    UINT16                  Length;
    UINT8                   Index;
    UINT8                   Flags;

} ACPI_IBFT_HEADER;


/* Values for Type field above */

enum AcpiIbftType
{
    ACPI_IBFT_TYPE_NOT_USED         = 0,
    ACPI_IBFT_TYPE_CONTROL          = 1,
    ACPI_IBFT_TYPE_INITIATOR        = 2,
    ACPI_IBFT_TYPE_NIC              = 3,
    ACPI_IBFT_TYPE_TARGET           = 4,
    ACPI_IBFT_TYPE_EXTENSIONS       = 5,
    ACPI_IBFT_TYPE_RESERVED         = 6     /* 6 and greater are reserved */
};


/* IBFT subtables */

typedef struct acpi_ibft_control
{
    ACPI_IBFT_HEADER        Header;
    UINT16                  Extensions;
    UINT16                  InitiatorOffset;
    UINT16                  Nic0Offset;
    UINT16                  Target0Offset;
    UINT16                  Nic1Offset;
    UINT16                  Target1Offset;

} ACPI_IBFT_CONTROL;

typedef struct acpi_ibft_initiator
{
    ACPI_IBFT_HEADER        Header;
    UINT8                   SnsServer[16];
    UINT8                   SlpServer[16];
    UINT8                   PrimaryServer[16];
    UINT8                   SecondaryServer[16];
    UINT16                  NameLength;
    UINT16                  NameOffset;

} ACPI_IBFT_INITIATOR;

typedef struct acpi_ibft_nic
{
    ACPI_IBFT_HEADER        Header;
    UINT8                   IpAddress[16];
    UINT8                   SubnetMaskPrefix;
    UINT8                   Origin;
    UINT8                   Gateway[16];
    UINT8                   PrimaryDns[16];
    UINT8                   SecondaryDns[16];
    UINT8                   Dhcp[16];
    UINT16                  Vlan;
    UINT8                   MacAddress[6];
    UINT16                  PciAddress;
    UINT16                  NameLength;
    UINT16                  NameOffset;

} ACPI_IBFT_NIC;

typedef struct acpi_ibft_target
{
    ACPI_IBFT_HEADER        Header;
    UINT8                   TargetIpAddress[16];
    UINT16                  TargetIpSocket;
    UINT8                   TargetBootLun[8];
    UINT8                   ChapType;
    UINT8                   NicAssociation;
    UINT16                  TargetNameLength;
    UINT16                  TargetNameOffset;
    UINT16                  ChapNameLength;
    UINT16                  ChapNameOffset;
    UINT16                  ChapSecretLength;
    UINT16                  ChapSecretOffset;
    UINT16                  ReverseChapNameLength;
    UINT16                  ReverseChapNameOffset;
    UINT16                  ReverseChapSecretLength;
    UINT16                  ReverseChapSecretOffset;

} ACPI_IBFT_TARGET;


/*******************************************************************************
 *
 * MADT - Multiple APIC Description Table
 *
 ******************************************************************************/

typedef struct acpi_table_madt
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  Address;            /* Physical address of local APIC */
    UINT32                  Flags;

} ACPI_TABLE_MADT;

/* Flags */

#define ACPI_MADT_PCAT_COMPAT       (1)         /* 00:    System also has dual 8259s */

/* Values for PCATCompat flag */

#define ACPI_MADT_DUAL_PIC          0
#define ACPI_MADT_MULTIPLE_APIC     1


/* Values for subtable type in ACPI_SUBTABLE_HEADER */

enum AcpiMadtType
{
    ACPI_MADT_TYPE_LOCAL_APIC           = 0,
    ACPI_MADT_TYPE_IO_APIC              = 1,
    ACPI_MADT_TYPE_INTERRUPT_OVERRIDE   = 2,
    ACPI_MADT_TYPE_NMI_SOURCE           = 3,
    ACPI_MADT_TYPE_LOCAL_APIC_NMI       = 4,
    ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE  = 5,
    ACPI_MADT_TYPE_IO_SAPIC             = 6,
    ACPI_MADT_TYPE_LOCAL_SAPIC          = 7,
    ACPI_MADT_TYPE_INTERRUPT_SOURCE     = 8,
    ACPI_MADT_TYPE_LOCAL_X2APIC         = 9,
    ACPI_MADT_TYPE_LOCAL_X2APIC_NMI     = 10,
    ACPI_MADT_TYPE_RESERVED             = 11    /* 11 and greater are reserved */
};


/*
 * MADT Sub-tables, correspond to Type in ACPI_SUBTABLE_HEADER
 */

/* 0: Processor Local APIC */

typedef struct acpi_madt_local_apic
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT8                   ProcessorId;        /* ACPI processor id */
    UINT8                   Id;                 /* Processor's local APIC id */
    UINT32                  LapicFlags;

} ACPI_MADT_LOCAL_APIC;

/* 1: IO APIC */

typedef struct acpi_madt_io_apic
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT8                   Id;                 /* I/O APIC ID */
    UINT8                   Reserved;           /* Reserved - must be zero */
    UINT32                  Address;            /* APIC physical address */
    UINT32                  GlobalIrqBase;      /* Global system interrupt where INTI lines start */

} ACPI_MADT_IO_APIC;

/* 2: Interrupt Override */

typedef struct acpi_madt_interrupt_override
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT8                   Bus;                /* 0 - ISA */
    UINT8                   SourceIrq;          /* Interrupt source (IRQ) */
    UINT32                  GlobalIrq;          /* Global system interrupt */
    UINT16                  IntiFlags;

} ACPI_MADT_INTERRUPT_OVERRIDE;

/* 3: NMI Source */

typedef struct acpi_madt_nmi_source
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT16                  IntiFlags;
    UINT32                  GlobalIrq;          /* Global system interrupt */

} ACPI_MADT_NMI_SOURCE;

/* 4: Local APIC NMI */

typedef struct acpi_madt_local_apic_nmi
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT8                   ProcessorId;        /* ACPI processor id */
    UINT16                  IntiFlags;
    UINT8                   Lint;               /* LINTn to which NMI is connected */

} ACPI_MADT_LOCAL_APIC_NMI;

/* 5: Address Override */

typedef struct acpi_madt_local_apic_override
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT16                  Reserved;           /* Reserved, must be zero */
    UINT64                  Address;            /* APIC physical address */

} ACPI_MADT_LOCAL_APIC_OVERRIDE;

/* 6: I/O Sapic */

typedef struct acpi_madt_io_sapic
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT8                   Id;                 /* I/O SAPIC ID */
    UINT8                   Reserved;           /* Reserved, must be zero */
    UINT32                  GlobalIrqBase;      /* Global interrupt for SAPIC start */
    UINT64                  Address;            /* SAPIC physical address */

} ACPI_MADT_IO_SAPIC;

/* 7: Local Sapic */

typedef struct acpi_madt_local_sapic
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT8                   ProcessorId;        /* ACPI processor id */
    UINT8                   Id;                 /* SAPIC ID */
    UINT8                   Eid;                /* SAPIC EID */
    UINT8                   Reserved[3];        /* Reserved, must be zero */
    UINT32                  LapicFlags;
    UINT32                  Uid;                /* Numeric UID - ACPI 3.0 */
    char                    UidString[1];       /* String UID  - ACPI 3.0 */

} ACPI_MADT_LOCAL_SAPIC;

/* 8: Platform Interrupt Source */

typedef struct acpi_madt_interrupt_source
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT16                  IntiFlags;
    UINT8                   Type;               /* 1=PMI, 2=INIT, 3=corrected */
    UINT8                   Id;                 /* Processor ID */
    UINT8                   Eid;                /* Processor EID */
    UINT8                   IoSapicVector;      /* Vector value for PMI interrupts */
    UINT32                  GlobalIrq;          /* Global system interrupt */
    UINT32                  Flags;              /* Interrupt Source Flags */

} ACPI_MADT_INTERRUPT_SOURCE;

/* Flags field above */

#define ACPI_MADT_CPEI_OVERRIDE     (1)

/* 9: Processor Local X2APIC (07/2008) */

typedef struct acpi_madt_local_x2apic
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT16                  Reserved;           /* Reserved - must be zero */
    UINT32                  LocalApicId;        /* Processor x2APIC ID  */
    UINT32                  LapicFlags;
    UINT32                  Uid;                /* ACPI processor UID */

} ACPI_MADT_LOCAL_X2APIC;

/* 10: Local X2APIC NMI (07/2008) */

typedef struct acpi_madt_local_x2apic_nmi
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT16                  IntiFlags;
    UINT32                  Uid;                /* ACPI processor UID */
    UINT8                   Lint;               /* LINTn to which NMI is connected */
    UINT8                   Reserved[3];        /* Reserved - must be zero */

} ACPI_MADT_LOCAL_X2APIC_NMI;


/*
 * Common flags fields for MADT subtables
 */

/* MADT Local APIC flags (LapicFlags) */

#define ACPI_MADT_ENABLED           (1)         /* 00: Processor is usable if set */

/* MADT MPS INTI flags (IntiFlags) */

#define ACPI_MADT_POLARITY_MASK     (3)         /* 00-01: Polarity of APIC I/O input signals */
#define ACPI_MADT_TRIGGER_MASK      (3<<2)      /* 02-03: Trigger mode of APIC input signals */

/* Values for MPS INTI flags */

#define ACPI_MADT_POLARITY_CONFORMS       0
#define ACPI_MADT_POLARITY_ACTIVE_HIGH    1
#define ACPI_MADT_POLARITY_RESERVED       2
#define ACPI_MADT_POLARITY_ACTIVE_LOW     3

#define ACPI_MADT_TRIGGER_CONFORMS        (0)
#define ACPI_MADT_TRIGGER_EDGE            (1<<2)
#define ACPI_MADT_TRIGGER_RESERVED        (2<<2)
#define ACPI_MADT_TRIGGER_LEVEL           (3<<2)


/*******************************************************************************
 *
 * MCFG - PCI Memory Mapped Configuration table and sub-table
 *
 ******************************************************************************/

typedef struct acpi_table_mcfg
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   Reserved[8];

} ACPI_TABLE_MCFG;


/* Subtable */

typedef struct acpi_mcfg_allocation
{
    UINT64                  Address;            /* Base address, processor-relative */
    UINT16                  PciSegment;         /* PCI segment group number */
    UINT8                   StartBusNumber;     /* Starting PCI Bus number */
    UINT8                   EndBusNumber;       /* Final PCI Bus number */
    UINT32                  Reserved;

} ACPI_MCFG_ALLOCATION;


/*******************************************************************************
 *
 * SBST - Smart Battery Specification Table
 *
 ******************************************************************************/

typedef struct acpi_table_sbst
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  WarningLevel;
    UINT32                  LowLevel;
    UINT32                  CriticalLevel;

} ACPI_TABLE_SBST;


/*******************************************************************************
 *
 * SLIT - System Locality Distance Information Table
 *
 ******************************************************************************/

typedef struct acpi_table_slit
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT64                  LocalityCount;
    UINT8                   Entry[1];           /* Real size = localities^2 */

} ACPI_TABLE_SLIT;


/*******************************************************************************
 *
 * SPCR - Serial Port Console Redirection table
 *
 ******************************************************************************/

typedef struct acpi_table_spcr
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   InterfaceType;      /* 0=full 16550, 1=subset of 16550 */
    UINT8                   Reserved[3];
    ACPI_GENERIC_ADDRESS    SerialPort;
    UINT8                   InterruptType;
    UINT8                   PcInterrupt;
    UINT32                  Interrupt;
    UINT8                   BaudRate;
    UINT8                   Parity;
    UINT8                   StopBits;
    UINT8                   FlowControl;
    UINT8                   TerminalType;
    UINT8                   Reserved1;
    UINT16                  PciDeviceId;
    UINT16                  PciVendorId;
    UINT8                   PciBus;
    UINT8                   PciDevice;
    UINT8                   PciFunction;
    UINT32                  PciFlags;
    UINT8                   PciSegment;
    UINT32                  Reserved2;

} ACPI_TABLE_SPCR;


/*******************************************************************************
 *
 * SPMI - Server Platform Management Interface table
 *
 ******************************************************************************/

typedef struct acpi_table_spmi
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   Reserved;
    UINT8                   InterfaceType;
    UINT16                  SpecRevision;       /* Version of IPMI */
    UINT8                   InterruptType;
    UINT8                   GpeNumber;          /* GPE assigned */
    UINT8                   Reserved1;
    UINT8                   PciDeviceFlag;
    UINT32                  Interrupt;
    ACPI_GENERIC_ADDRESS    IpmiRegister;
    UINT8                   PciSegment;
    UINT8                   PciBus;
    UINT8                   PciDevice;
    UINT8                   PciFunction;

} ACPI_TABLE_SPMI;


/*******************************************************************************
 *
 * SRAT - System Resource Affinity Table
 *
 ******************************************************************************/

typedef struct acpi_table_srat
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  TableRevision;      /* Must be value '1' */
    UINT64                  Reserved;           /* Reserved, must be zero */

} ACPI_TABLE_SRAT;

/* Values for subtable type in ACPI_SUBTABLE_HEADER */

enum AcpiSratType
{
    ACPI_SRAT_TYPE_CPU_AFFINITY         = 0,
    ACPI_SRAT_TYPE_MEMORY_AFFINITY      = 1,
    ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY  = 2,
    ACPI_SRAT_TYPE_RESERVED             = 3     /* 3 and greater are reserved */
};

/*
 * SRAT Sub-tables, correspond to Type in ACPI_SUBTABLE_HEADER
 */

/* 0: Processor Local APIC/SAPIC Affinity */

typedef struct acpi_srat_cpu_affinity
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT8                   ProximityDomainLo;
    UINT8                   ApicId;
    UINT32                  Flags;
    UINT8                   LocalSapicEid;
    UINT8                   ProximityDomainHi[3];
    UINT32                  Reserved;           /* Reserved, must be zero */

} ACPI_SRAT_CPU_AFFINITY;

/* 1: Memory Affinity */

typedef struct acpi_srat_mem_affinity
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT32                  ProximityDomain;
    UINT16                  Reserved;           /* Reserved, must be zero */
    UINT64                  BaseAddress;
    UINT64                  Length;
    UINT32                  Reserved1;
    UINT32                  Flags;
    UINT64                  Reserved2;          /* Reserved, must be zero */

} ACPI_SRAT_MEM_AFFINITY;

/* Flags */

#define ACPI_SRAT_MEM_ENABLED       (1)         /* 00: Use affinity structure */
#define ACPI_SRAT_MEM_HOT_PLUGGABLE (1<<1)      /* 01: Memory region is hot pluggable */
#define ACPI_SRAT_MEM_NON_VOLATILE  (1<<2)      /* 02: Memory region is non-volatile */

/* 2: Processor Local X2_APIC Affinity (07/2008) */

typedef struct acpi_srat_x2apic_cpu_affinity
{
    ACPI_SUBTABLE_HEADER    Header;
    UINT16                  Reserved;           /* Reserved, must be zero */
    UINT32                  ProximityDomain;
    UINT32                  ApicId;
    UINT32                  Flags;

} ACPI_SRAT_X2APIC_CPU_AFFINITY;

/* Flags for ACPI_SRAT_CPU_AFFINITY and ACPI_SRAT_X2APIC_CPU_AFFINITY */

#define ACPI_SRAT_CPU_ENABLED       (1)         /* 00: Use affinity structure */


/*******************************************************************************
 *
 * TCPA - Trusted Computing Platform Alliance table
 *
 ******************************************************************************/

typedef struct acpi_table_tcpa
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT16                  Reserved;
    UINT32                  MaxLogLength;       /* Maximum length for the event log area */
    UINT64                  LogAddress;         /* Address of the event log area */

} ACPI_TABLE_TCPA;


/*******************************************************************************
 *
 * UEFI - UEFI Boot optimization Table
 *
 ******************************************************************************/

typedef struct acpi_table_uefi
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT8                   Identifier[16];     /* UUID identifier */
    UINT16                  DataOffset;         /* Offset of remaining data in table */
    UINT8                   Data;

} ACPI_TABLE_UEFI;


/*******************************************************************************
 *
 * WDAT - Watchdog Action Table
 *
 ******************************************************************************/

typedef struct acpi_table_wdat
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  HeaderLength;       /* Watchdog Header Length */
    UINT16                  PciSegment;         /* PCI Segment number */
    UINT8                   PciBus;             /* PCI Bus number */
    UINT8                   PciDevice;          /* PCI Device number */
    UINT8                   PciFunction;        /* PCI Function number */
    UINT8                   Reserved[3];
    UINT32                  TimerPeriod;        /* Period of one timer count (msec) */
    UINT32                  MaxCount;           /* Maximum counter value supported */
    UINT32                  MinCount;           /* Minimum counter value */
    UINT8                   Flags;
    UINT8                   Reserved2[3];
    UINT32                  Entries;            /* Number of watchdog entries that follow */

} ACPI_TABLE_WDAT;

/* WDAT Instruction Entries (actions) */

typedef struct acpi_wdat_entry
{
    ACPI_WHEA_HEADER        WheaHeader;         /* Common header for WHEA tables */

} ACPI_WDAT_ENTRY;

/* Values for Action field above */

enum AcpiWdatActions
{
    ACPI_WDAT_RESET                 = 1,
    ACPI_WDAT_GET_CURRENT_COUNTDOWN = 4,
    ACPI_WDAT_GET_COUNTDOWN         = 5,
    ACPI_WDAT_SET_COUNTDOWN         = 6,
    ACPI_WDAT_GET_RUNNING_STATE     = 8,
    ACPI_WDAT_SET_RUNNING_STATE     = 9,
    ACPI_WDAT_GET_STOPPED_STATE     = 10,
    ACPI_WDAT_SET_STOPPED_STATE     = 11,
    ACPI_WDAT_GET_REBOOT            = 16,
    ACPI_WDAT_SET_REBOOT            = 17,
    ACPI_WDAT_GET_SHUTDOWN          = 18,
    ACPI_WDAT_SET_SHUTDOWN          = 19,
    ACPI_WDAT_GET_STATUS            = 32,
    ACPI_WDAT_SET_STATUS            = 33,
    ACPI_WDAT_ACTION_RESERVED       = 34    /* 34 and greater are reserved */
};

/* Values for Instruction field above */

enum AcpiWdatInstructions
{
    ACPI_WDAT_READ_VALUE            = 0,
    ACPI_WDAT_READ_COUNTDOWN        = 1,
    ACPI_WDAT_WRITE_VALUE           = 2,
    ACPI_WDAT_WRITE_COUNTDOWN       = 3,
    ACPI_WDAT_INSTRUCTION_RESERVED  = 4,    /* 4 and greater are reserved */
    ACPI_WDAT_PRESERVE_REGISTER     = 0x80  /* Except for this value */
};


/*******************************************************************************
 *
 * WDRT - Watchdog Resource Table
 *
 ******************************************************************************/

typedef struct acpi_table_wdrt
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  HeaderLength;       /* Watchdog Header Length */
    UINT8                   PciSegment;         /* PCI Segment number */
    UINT8                   PciBus;             /* PCI Bus number */
    UINT8                   PciDevice;          /* PCI Device number */
    UINT8                   PciFunction;        /* PCI Function number */
    UINT32                  TimerPeriod;        /* Period of one timer count (msec) */
    UINT32                  MaxCount;           /* Maximum counter value supported */
    UINT32                  MinCount;           /* Minimum counter value */
    UINT8                   Flags;
    UINT8                   Reserved[3];
    UINT32                  Entries;            /* Number of watchdog entries that follow */

} ACPI_TABLE_WDRT;

/* Flags */

#define ACPI_WDRT_TIMER_ENABLED     (1)         /* 00: Timer enabled */


/* Reset to default packing */

#pragma pack()

#endif /* __ACTBL1_H__ */
