/******************************************************************************
 *
 * Name: actbl71.h - IA-64 Extensions to the ACPI Spec Rev. 0.71
 *                   This file includes tables specific to this
 *                   specification revision.
 *       $Revision: 12 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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

#ifndef __ACTBL71_H__
#define __ACTBL71_H__


/* 0.71 FADT AddressSpace data item bitmasks defines */
/* If the associated bit is zero then it is in memory space else in io space */

#define SMI_CMD_ADDRESS_SPACE       0x01
#define PM1_BLK_ADDRESS_SPACE       0x02
#define PM2_CNT_BLK_ADDRESS_SPACE   0x04
#define PM_TMR_BLK_ADDRESS_SPACE    0x08
#define GPE0_BLK_ADDRESS_SPACE      0x10
#define GPE1_BLK_ADDRESS_SPACE      0x20

/* Only for clarity in declarations */

typedef UINT64              IO_ADDRESS;


#pragma pack(1)
typedef struct  /* Root System Descriptor Pointer */
{
    NATIVE_CHAR             Signature [8];          /* contains "RSD PTR " */
    UINT8                   Checksum;               /* to make sum of struct == 0 */
    NATIVE_CHAR             OemId [6];              /* OEM identification */
    UINT8                   Reserved;               /* Must be 0 for 1.0, 2 for 2.0 */
    UINT64                  RsdtPhysicalAddress;    /* 64-bit physical address of RSDT */
} RSDP_DESCRIPTOR_REV071;


/*****************************************/
/* IA64 Extensions to ACPI Spec Rev 0.71 */
/* for the Root System Description Table */
/*****************************************/
typedef struct
{
    ACPI_TABLE_HEADER   header;                 /* Table header */
    UINT32              ReservedPad;            /* IA64 alignment, must be 0 */
    UINT64              TableOffsetEntry [1];  /* Array of pointers to other */
                                                /* tables' headers */
} RSDT_DESCRIPTOR_REV071;


/*******************************************/
/* IA64 Extensions to ACPI Spec Rev 0.71   */
/* for the Firmware ACPI Control Structure */
/*******************************************/
typedef struct
{
    NATIVE_CHAR         Signature[4];         /* signature "FACS" */
    UINT32              Length;               /* length of structure, in bytes */
    UINT32              HardwareSignature;    /* hardware configuration signature */
    UINT32              Reserved4;            /* must be 0 */
    UINT64              FirmwareWakingVector; /* ACPI OS waking vector */
    UINT64              GlobalLock;           /* Global Lock */
    UINT32_BIT          S4Bios_f      : 1;    /* Indicates if S4BIOS support is present */
    UINT32_BIT          Reserved1     : 31;   /* must be 0 */
    UINT8               Reserved3 [28];       /* reserved - must be zero */

} FACS_DESCRIPTOR_REV071;


/******************************************/
/* IA64 Extensions to ACPI Spec Rev 0.71  */
/* for the Fixed ACPI Description Table   */
/******************************************/
typedef struct
{
    ACPI_TABLE_HEADER   header;             /* table header */
    UINT32              ReservedPad;        /* IA64 alignment, must be 0 */
    UINT64              FirmwareCtrl;       /* 64-bit Physical address of FACS */
    UINT64              Dsdt;               /* 64-bit Physical address of DSDT */
    UINT8               Model;              /* System Interrupt Model */
    UINT8               AddressSpace;       /* Address Space Bitmask */
    UINT16              SciInt;             /* System vector of SCI interrupt */
    UINT8               AcpiEnable;         /* value to write to smi_cmd to enable ACPI */
    UINT8               AcpiDisable;        /* value to write to smi_cmd to disable ACPI */
    UINT8               S4BiosReq;          /* Value to write to SMI CMD to enter S4BIOS state */
    UINT8               Reserved2;          /* reserved - must be zero */
    UINT64              SmiCmd;             /* Port address of SMI command port */
    UINT64              Pm1aEvtBlk;         /* Port address of Power Mgt 1a AcpiEvent Reg Blk */
    UINT64              Pm1bEvtBlk;         /* Port address of Power Mgt 1b AcpiEvent Reg Blk */
    UINT64              Pm1aCntBlk;         /* Port address of Power Mgt 1a Control Reg Blk */
    UINT64              Pm1bCntBlk;         /* Port address of Power Mgt 1b Control Reg Blk */
    UINT64              Pm2CntBlk;          /* Port address of Power Mgt 2 Control Reg Blk */
    UINT64              PmTmrBlk;           /* Port address of Power Mgt Timer Ctrl Reg Blk */
    UINT64              Gpe0Blk;            /* Port addr of General Purpose AcpiEvent 0 Reg Blk */
    UINT64              Gpe1Blk;            /* Port addr of General Purpose AcpiEvent 1 Reg Blk */
    UINT8               Pm1EvtLen;          /* Byte Length of ports at pm1X_evt_blk */
    UINT8               Pm1CntLen;          /* Byte Length of ports at pm1X_cnt_blk */
    UINT8               Pm2CntLen;          /* Byte Length of ports at pm2_cnt_blk */
    UINT8               PmTmLen;            /* Byte Length of ports at pm_tm_blk */
    UINT8               Gpe0BlkLen;         /* Byte Length of ports at gpe0_blk */
    UINT8               Gpe1BlkLen;         /* Byte Length of ports at gpe1_blk */
    UINT8               Gpe1Base;           /* offset in gpe model where gpe1 events start */
    UINT8               Reserved3;          /* reserved */
    UINT16              Plvl2Lat;           /* worst case HW latency to enter/exit C2 state */
    UINT16              Plvl3Lat;           /* worst case HW latency to enter/exit C3 state */
    UINT8               DayAlrm;            /* index to day-of-month alarm in RTC CMOS RAM */
    UINT8               MonAlrm;            /* index to month-of-year alarm in RTC CMOS RAM */
    UINT8               Century;            /* index to century in RTC CMOS RAM */
    UINT8               Reserved4;          /* reserved */
    UINT32_BIT          FlushCash   : 1;    /* PAL_FLUSH_CACHE is correctly supported */
    UINT32_BIT          Reserved5   : 1;    /* reserved - must be zero */
    UINT32_BIT          ProcC1      : 1;    /* all processors support C1 state */
    UINT32_BIT          Plvl2Up     : 1;    /* C2 state works on MP system */
    UINT32_BIT          PwrButton   : 1;    /* Power button is handled as a generic feature */
    UINT32_BIT          SleepButton : 1;    /* Sleep button is handled as a generic feature, or not present */
    UINT32_BIT          FixedRTC    : 1;    /* RTC wakeup stat not in fixed register space */
    UINT32_BIT          Rtcs4       : 1;    /* RTC wakeup stat not possible from S4 */
    UINT32_BIT          TmrValExt   : 1;    /* tmr_val is 32 bits */
    UINT32_BIT          DockCap     : 1;    /* Supports Docking */
    UINT32_BIT          Reserved6   : 22;    /* reserved - must be zero */

}  FADT_DESCRIPTOR_REV071;

#pragma pack()

#endif /* __ACTBL71_H__ */

