/******************************************************************************
 *
 * Name: actbl.h - Table data structures defined in ACPI specification
 *       $Revision: 58 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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

#ifndef __ACTBL_H__
#define __ACTBL_H__


/*
 *  Values for description table header signatures
 */
#define RSDP_NAME               "RSDP"
#define RSDP_SIG                "RSD PTR "  /* RSDT Pointer signature */
#define APIC_SIG                "APIC"      /* Multiple APIC Description Table */
#define DSDT_SIG                "DSDT"      /* Differentiated System Description Table */
#define FADT_SIG                "FACP"      /* Fixed ACPI Description Table */
#define FACS_SIG                "FACS"      /* Firmware ACPI Control Structure */
#define PSDT_SIG                "PSDT"      /* Persistent System Description Table */
#define RSDT_SIG                "RSDT"      /* Root System Description Table */
#define XSDT_SIG                "XSDT"      /* Extended  System Description Table */
#define SSDT_SIG                "SSDT"      /* Secondary System Description Table */
#define SBST_SIG                "SBST"      /* Smart Battery Specification Table */
#define SPIC_SIG                "SPIC"      /* IOSAPIC table */
#define BOOT_SIG                "BOOT"      /* Boot table */


#define GL_OWNED                0x02        /* Ownership of global lock is bit 1 */

/* values of Mapic.Model */

#define DUAL_PIC                0
#define MULTIPLE_APIC           1

/* values of Type in APIC_HEADER */

#define APIC_PROC               0
#define APIC_IO                 1


/*
 * Common table types.  The base code can remain
 * constant if the underlying tables are changed
 */
#define RSDT_DESCRIPTOR         RSDT_DESCRIPTOR_REV2
#define XSDT_DESCRIPTOR         XSDT_DESCRIPTOR_REV2
#define FACS_DESCRIPTOR         FACS_DESCRIPTOR_REV2
#define FADT_DESCRIPTOR         FADT_DESCRIPTOR_REV2


#pragma pack(1)

/*
 * Architecture-independent tables
 * The architecture dependent tables are in separate files
 */
typedef struct rsdp_descriptor /* Root System Descriptor Pointer */
{
    char                    Signature [8];          /* ACPI signature, contains "RSD PTR " */
    UINT8                   Checksum;               /* To make sum of struct == 0 */
    char                    OemId [6];              /* OEM identification */
    UINT8                   Revision;               /* Must be 0 for 1.0, 2 for 2.0 */
    UINT32                  RsdtPhysicalAddress;    /* 32-bit physical address of RSDT */
    UINT32                  Length;                 /* XSDT Length in bytes including hdr */
    UINT64                  XsdtPhysicalAddress;    /* 64-bit physical address of XSDT */
    UINT8                   ExtendedChecksum;       /* Checksum of entire table */
    char                    Reserved [3];           /* Reserved field must be 0 */

} RSDP_DESCRIPTOR;


typedef struct acpi_table_header /* ACPI common table header */
{
    char                    Signature [4];          /* ACPI signature (4 ASCII characters) */
    UINT32                  Length;                 /* Length of table, in bytes, including header */
    UINT8                   Revision;               /* ACPI Specification minor version # */
    UINT8                   Checksum;               /* To make sum of entire table == 0 */
    char                    OemId [6];              /* OEM identification */
    char                    OemTableId [8];         /* OEM table identification */
    UINT32                  OemRevision;            /* OEM revision number */
    char                    AslCompilerId [4];      /* ASL compiler vendor ID */
    UINT32                  AslCompilerRevision;    /* ASL compiler revision number */

} ACPI_TABLE_HEADER;


typedef struct acpi_common_facs  /* Common FACS for internal use */
{
    UINT32                  *GlobalLock;
    UINT64                  *FirmwareWakingVector;
    UINT8                   VectorWidth;

} ACPI_COMMON_FACS;


typedef struct apic_table
{
    ACPI_TABLE_HEADER       Header;                 /* ACPI table header */
    UINT32                  LocalApicAddress;       /* Physical address for accessing local APICs */
    UINT32_BIT              PCATCompat      : 1;    /* a one indicates system also has dual 8259s */
    UINT32_BIT              Reserved1       : 31;

} APIC_TABLE;


typedef struct apic_header
{
    UINT8                   Type;                   /* APIC type.  Either APIC_PROC or APIC_IO */
    UINT8                   Length;                 /* Length of APIC structure */

} APIC_HEADER;


typedef struct processor_apic
{
    APIC_HEADER             Header;
    UINT8                   ProcessorApicId;        /* ACPI processor id */
    UINT8                   LocalApicId;            /* Processor's local APIC id */
    UINT32_BIT              ProcessorEnabled: 1;    /* Processor is usable if set */
    UINT32_BIT              Reserved1       : 31;

} PROCESSOR_APIC;


typedef struct io_apic
{
    APIC_HEADER             Header;
    UINT8                   IoApicId;               /* I/O APIC ID */
    UINT8                   Reserved;               /* Reserved - must be zero */
    UINT32                  IoApicAddress;          /* APIC's physical address */
    UINT32                  Vector;                 /* Interrupt vector index where INTI
                                                     * lines start */
} IO_APIC;


/*
 *  IA64 TBD:  Add SAPIC Tables
 */

/*
 *  IA64 TBD:   Modify Smart Battery Description to comply with ACPI IA64
 *              extensions.
 */
typedef struct smart_battery_description_table
{
    ACPI_TABLE_HEADER       Header;
    UINT32                  WarningLevel;
    UINT32                  LowLevel;
    UINT32                  CriticalLevel;

} SMART_BATTERY_DESCRIPTION_TABLE;


#pragma pack()


/*
 * ACPI Table information.  We save the table address, length,
 * and type of memory allocation (mapped or allocated) for each
 * table for 1) when we exit, and 2) if a new table is installed
 */
#define ACPI_MEM_NOT_ALLOCATED  0
#define ACPI_MEM_ALLOCATED      1
#define ACPI_MEM_MAPPED         2

/* Definitions for the Flags bitfield member of ACPI_TABLE_SUPPORT */

#define ACPI_TABLE_SINGLE       0x00
#define ACPI_TABLE_MULTIPLE     0x01
#define ACPI_TABLE_EXECUTABLE   0x02

#define ACPI_TABLE_ROOT         0x00
#define ACPI_TABLE_PRIMARY      0x10
#define ACPI_TABLE_SECONDARY    0x20
#define ACPI_TABLE_ALL          0x30
#define ACPI_TABLE_TYPE_MASK    0x30

/* Data about each known table type */

typedef struct acpi_table_support
{
    char                    *Name;
    char                    *Signature;
    void                    **GlobalPtr;
    UINT8                   SigLength;
    UINT8                   Flags;

} ACPI_TABLE_SUPPORT;


/*
 * Get the architecture-specific tables
 */
#include "actbl1.h"   /* Acpi 1.0 table definitions */
#include "actbl2.h"   /* Acpi 2.0 table definitions */

#endif /* __ACTBL_H__ */
