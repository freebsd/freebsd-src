/******************************************************************************
 *
 * Name: actbl2.h - ACPI Specification Revision 2.0 Tables
 *       $Revision: 1.45 $
 *
 *****************************************************************************/

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

#ifndef __ACTBL2_H__
#define __ACTBL2_H__

/*
 * Prefered Power Management Profiles
 */
#define PM_UNSPECIFIED                  0
#define PM_DESKTOP                      1
#define PM_MOBILE                       2
#define PM_WORKSTATION                  3
#define PM_ENTERPRISE_SERVER            4
#define PM_SOHO_SERVER                  5
#define PM_APPLIANCE_PC                 6

/*
 * ACPI Boot Arch Flags
 */
#define BAF_LEGACY_DEVICES              0x0001
#define BAF_8042_KEYBOARD_CONTROLLER    0x0002

#define FADT2_REVISION_ID               3
#define FADT2_MINUS_REVISION_ID         2


#pragma pack(1)

/*
 * ACPI 2.0 Root System Description Table (RSDT)
 */
typedef struct rsdt_descriptor_rev2
{
    ACPI_TABLE_HEADER_DEF                           /* ACPI common table header */
    UINT32                  TableOffsetEntry[1];    /* Array of pointers to ACPI tables */

} RSDT_DESCRIPTOR_REV2;


/*
 * ACPI 2.0 Extended System Description Table (XSDT)
 */
typedef struct xsdt_descriptor_rev2
{
    ACPI_TABLE_HEADER_DEF                           /* ACPI common table header */
    UINT64                  TableOffsetEntry[1];    /* Array of pointers to ACPI tables */

} XSDT_DESCRIPTOR_REV2;


/*
 * ACPI 2.0 Firmware ACPI Control Structure (FACS)
 */
typedef struct facs_descriptor_rev2
{
    char                    Signature[4];           /* ASCII table signature */
    UINT32                  Length;                 /* Length of structure, in bytes */
    UINT32                  HardwareSignature;      /* Hardware configuration signature */
    UINT32                  FirmwareWakingVector;   /* 32-bit physical address of the Firmware Waking Vector. */
    UINT32                  GlobalLock;             /* Global Lock used to synchronize access to shared hardware resources */

    /* Flags (32 bits) */

    UINT8_BIT               S4Bios_f        : 1;    /* 00:    S4BIOS support is present */
    UINT8_BIT                               : 7;    /* 01-07: Reserved, must be zero */
    UINT8                   Reserved1[3];           /* 08-31: Reserved, must be zero */

    UINT64                  XFirmwareWakingVector;  /* 64-bit physical address of the Firmware Waking Vector. */
    UINT8                   Version;                /* Version of this table */
    UINT8                   Reserved3[31];          /* Reserved, must be zero */

} FACS_DESCRIPTOR_REV2;


/*
 * ACPI 2.0+ Generic Address Structure (GAS)
 */
typedef struct acpi_generic_address
{
    UINT8                   AddressSpaceId;         /* Address space where struct or register exists. */
    UINT8                   RegisterBitWidth;       /* Size in bits of given register */
    UINT8                   RegisterBitOffset;      /* Bit offset within the register */
    UINT8                   AccessWidth;            /* Minimum Access size (ACPI 3.0) */
    UINT64                  Address;                /* 64-bit address of struct or register */

} ACPI_GENERIC_ADDRESS;


#define FADT_REV2_COMMON \
    UINT32                  V1_FirmwareCtrl;    /* 32-bit physical address of FACS */ \
    UINT32                  V1_Dsdt;            /* 32-bit physical address of DSDT */ \
    UINT8                   Reserved1;          /* System Interrupt Model isn't used in ACPI 2.0*/ \
    UINT8                   Prefer_PM_Profile;  /* Conveys preferred power management profile to OSPM. */ \
    UINT16                  SciInt;             /* System vector of SCI interrupt */ \
    UINT32                  SmiCmd;             /* Port address of SMI command port */ \
    UINT8                   AcpiEnable;         /* Value to write to smi_cmd to enable ACPI */ \
    UINT8                   AcpiDisable;        /* Value to write to smi_cmd to disable ACPI */ \
    UINT8                   S4BiosReq;          /* Value to write to SMI CMD to enter S4BIOS state */ \
    UINT8                   PstateCnt;          /* Processor performance state control*/ \
    UINT32                  V1_Pm1aEvtBlk;      /* Port address of Power Mgt 1a AcpiEvent Reg Blk */ \
    UINT32                  V1_Pm1bEvtBlk;      /* Port address of Power Mgt 1b AcpiEvent Reg Blk */ \
    UINT32                  V1_Pm1aCntBlk;      /* Port address of Power Mgt 1a Control Reg Blk */ \
    UINT32                  V1_Pm1bCntBlk;      /* Port address of Power Mgt 1b Control Reg Blk */ \
    UINT32                  V1_Pm2CntBlk;       /* Port address of Power Mgt 2 Control Reg Blk */ \
    UINT32                  V1_PmTmrBlk;        /* Port address of Power Mgt Timer Ctrl Reg Blk */ \
    UINT32                  V1_Gpe0Blk;         /* Port addr of General Purpose AcpiEvent 0 Reg Blk */ \
    UINT32                  V1_Gpe1Blk;         /* Port addr of General Purpose AcpiEvent 1 Reg Blk */ \
    UINT8                   Pm1EvtLen;          /* Byte Length of ports at pm1X_evt_blk */ \
    UINT8                   Pm1CntLen;          /* Byte Length of ports at pm1X_cnt_blk */ \
    UINT8                   Pm2CntLen;          /* Byte Length of ports at pm2_cnt_blk */ \
    UINT8                   PmTmLen;            /* Byte Length of ports at pm_tm_blk */ \
    UINT8                   Gpe0BlkLen;         /* Byte Length of ports at gpe0_blk */ \
    UINT8                   Gpe1BlkLen;         /* Byte Length of ports at gpe1_blk */ \
    UINT8                   Gpe1Base;           /* Offset in gpe model where gpe1 events start */ \
    UINT8                   CstCnt;             /* Support for the _CST object and C States change notification.*/ \
    UINT16                  Plvl2Lat;           /* Worst case HW latency to enter/exit C2 state */ \
    UINT16                  Plvl3Lat;           /* Worst case HW latency to enter/exit C3 state */ \
    UINT16                  FlushSize;          /* Number of flush strides that need to be read */ \
    UINT16                  FlushStride;        /* Processor's memory cache line width, in bytes */ \
    UINT8                   DutyOffset;         /* Processor's duty cycle index in processor's P_CNT reg*/ \
    UINT8                   DutyWidth;          /* Processor's duty cycle value bit width in P_CNT register.*/ \
    UINT8                   DayAlrm;            /* Index to day-of-month alarm in RTC CMOS RAM */ \
    UINT8                   MonAlrm;            /* Index to month-of-year alarm in RTC CMOS RAM */ \
    UINT8                   Century;            /* Index to century in RTC CMOS RAM */ \
    UINT16                  IapcBootArch;       /* IA-PC Boot Architecture Flags. See Table 5-10 for description*/

/*
 * ACPI 2.0+ Fixed ACPI Description Table (FADT)
 */
typedef struct fadt_descriptor_rev2
{
    ACPI_TABLE_HEADER_DEF                       /* ACPI common table header */
    FADT_REV2_COMMON
    UINT8                   Reserved2;          /* Reserved, must be zero */

    /* Flags (32 bits) */

    UINT8_BIT               WbInvd      : 1;    /* 00:    The wbinvd instruction works properly */
    UINT8_BIT               WbInvdFlush : 1;    /* 01:    The wbinvd flushes but does not invalidate */
    UINT8_BIT               ProcC1      : 1;    /* 02:    All processors support C1 state */
    UINT8_BIT               Plvl2Up     : 1;    /* 03:    C2 state works on MP system */
    UINT8_BIT               PwrButton   : 1;    /* 04:    Power button is handled as a generic feature */
    UINT8_BIT               SleepButton : 1;    /* 05:    Sleep button is handled as a generic feature, or not present */
    UINT8_BIT               FixedRTC    : 1;    /* 06:    RTC wakeup stat not in fixed register space */
    UINT8_BIT               Rtcs4       : 1;    /* 07:    RTC wakeup stat not possible from S4 */
    UINT8_BIT               TmrValExt   : 1;    /* 08:    tmr_val is 32 bits 0=24-bits */
    UINT8_BIT               DockCap     : 1;    /* 09:    Docking supported */
    UINT8_BIT               ResetRegSup : 1;    /* 10:    System reset via the FADT RESET_REG supported */
    UINT8_BIT               SealedCase  : 1;    /* 11:    No internal expansion capabilities and case is sealed */
    UINT8_BIT               Headless    : 1;    /* 12:    No local video capabilities or local input devices */
    UINT8_BIT               CpuSwSleep  : 1;    /* 13:    Must execute native instruction after writing SLP_TYPx register */

    UINT8_BIT               PciExpWak                           : 1; /* 14:    System supports PCIEXP_WAKE (STS/EN) bits (ACPI 3.0) */
    UINT8_BIT               UsePlatformClock                    : 1; /* 15:    OSPM should use platform-provided timer (ACPI 3.0) */
    UINT8_BIT               S4RtcStsValid                       : 1; /* 16:    Contents of RTC_STS valid after S4 wake (ACPI 3.0) */
    UINT8_BIT               RemotePowerOnCapable                : 1; /* 17:    System is compatible with remote power on (ACPI 3.0) */
    UINT8_BIT               ForceApicClusterModel               : 1; /* 18:    All local APICs must use cluster model (ACPI 3.0) */
    UINT8_BIT               ForceApicPhysicalDestinationMode    : 1; /* 19:    All local xAPICs must use physical dest mode (ACPI 3.0) */
    UINT8_BIT                                                   : 4; /* 20-23: Reserved, must be zero */
    UINT8                   Reserved3;                               /* 24-31: Reserved, must be zero */

    ACPI_GENERIC_ADDRESS    ResetRegister;      /* Reset register address in GAS format */
    UINT8                   ResetValue;         /* Value to write to the ResetRegister port to reset the system */
    UINT8                   Reserved4[3];       /* These three bytes must be zero */
    UINT64                  XFirmwareCtrl;      /* 64-bit physical address of FACS */
    UINT64                  XDsdt;              /* 64-bit physical address of DSDT */
    ACPI_GENERIC_ADDRESS    XPm1aEvtBlk;        /* Extended Power Mgt 1a AcpiEvent Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1bEvtBlk;        /* Extended Power Mgt 1b AcpiEvent Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1aCntBlk;        /* Extended Power Mgt 1a Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1bCntBlk;        /* Extended Power Mgt 1b Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm2CntBlk;         /* Extended Power Mgt 2 Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPmTmrBlk;          /* Extended Power Mgt Timer Ctrl Reg Blk address */
    ACPI_GENERIC_ADDRESS    XGpe0Blk;           /* Extended General Purpose AcpiEvent 0 Reg Blk address */
    ACPI_GENERIC_ADDRESS    XGpe1Blk;           /* Extended General Purpose AcpiEvent 1 Reg Blk address */

} FADT_DESCRIPTOR_REV2;


/* "Down-revved" ACPI 2.0 FADT descriptor */

typedef struct fadt_descriptor_rev2_minus
{
    ACPI_TABLE_HEADER_DEF                       /* ACPI common table header */
    FADT_REV2_COMMON
    UINT8                   Reserved2;          /* Reserved, must be zero */
    UINT32                  Flags;
    ACPI_GENERIC_ADDRESS    ResetRegister;      /* Reset register address in GAS format */
    UINT8                   ResetValue;         /* Value to write to the ResetRegister port to reset the system. */
    UINT8                   Reserved7[3];       /* Reserved, must be zero */

} FADT_DESCRIPTOR_REV2_MINUS;


/* ECDT - Embedded Controller Boot Resources Table */

typedef struct ec_boot_resources
{
    ACPI_TABLE_HEADER_DEF
    ACPI_GENERIC_ADDRESS    EcControl;          /* Address of EC command/status register */
    ACPI_GENERIC_ADDRESS    EcData;             /* Address of EC data register */
    UINT32                  Uid;                /* Unique ID - must be same as the EC _UID method */
    UINT8                   GpeBit;             /* The GPE for the EC */
    UINT8                   EcId[1];            /* Full namepath of the EC in the ACPI namespace */

} EC_BOOT_RESOURCES;


/* SRAT - System Resource Affinity Table */

typedef struct static_resource_alloc
{
    UINT8                   Type;
    UINT8                   Length;
    UINT8                   ProximityDomainLo;
    UINT8                   ApicId;

    /* Flags (32 bits) */

    UINT8_BIT               Enabled         :1; /* 00:    Use affinity structure */
    UINT8_BIT                               :7; /* 01-07: Reserved, must be zero */
    UINT8                   Reserved3[3];       /* 08-31: Reserved, must be zero */

    UINT8                   LocalSapicEid;
    UINT8                   ProximityDomainHi[3];
    UINT32                  Reserved4;          /* Reserved, must be zero */

} STATIC_RESOURCE_ALLOC;

typedef struct memory_affinity
{
    UINT8                   Type;
    UINT8                   Length;
    UINT32                  ProximityDomain;
    UINT16                  Reserved3;
    UINT64                  BaseAddress;
    UINT64                  AddressLength;
    UINT32                  Reserved4;

    /* Flags (32 bits) */

    UINT8_BIT               Enabled         :1; /* 00:    Use affinity structure */
    UINT8_BIT               HotPluggable    :1; /* 01:    Memory region is hot pluggable */
    UINT8_BIT               NonVolatile     :1; /* 02:    Memory is non-volatile */
    UINT8_BIT                               :5; /* 03-07: Reserved, must be zero */
    UINT8                   Reserved5[3];       /* 08-31: Reserved, must be zero */

    UINT64                  Reserved6;          /* Reserved, must be zero */

} MEMORY_AFFINITY;

typedef struct system_resource_affinity
{
    ACPI_TABLE_HEADER_DEF
    UINT32                  Reserved1;          /* Must be value '1' */
    UINT64                  Reserved2;          /* Reserved, must be zero */

} SYSTEM_RESOURCE_AFFINITY;


/* SLIT - System Locality Distance Information Table */

typedef struct system_locality_info
{
    ACPI_TABLE_HEADER_DEF
    UINT64                  LocalityCount;
    UINT8                   Entry[1][1];

} SYSTEM_LOCALITY_INFO;


#pragma pack()

#endif /* __ACTBL2_H__ */

