/******************************************************************************
 *
 * Module Name: extables - ACPI tables for Example program
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

#include "examples.h"
#include "actables.h"

#define _COMPONENT          ACPI_EXAMPLE
        ACPI_MODULE_NAME    ("extables")


/******************************************************************************
 *
 * ACPICA Example tables and table setup
 *
 * This module contains the ACPI tables used for the example program. The
 * original source code for the tables appears at the end of the module.
 *
 *****************************************************************************/


/* These tables will be modified at runtime */

unsigned char RsdpCode[] =
{
    0x52,0x53,0x44,0x20,0x50,0x54,0x52,0x20,  /* 00000000    "RSD PTR " */
    0x43,0x49,0x4E,0x54,0x45,0x4C,0x20,0x02,  /* 00000008    "CINTEL ." */
    0x00,0x00,0x00,0x00,0x24,0x00,0x00,0x00,  /* 00000010    "....$..." */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000018    "........" */
    0xDC,0x00,0x00,0x00                       /* 00000020    "...."     */
};

unsigned char RsdtCode[] =
{
    0x52,0x53,0x44,0x54,0x28,0x00,0x00,0x00,  /* 00000000    "RSDT(..." */
    0x01,0x10,0x49,0x4E,0x54,0x45,0x4C,0x20,  /* 00000008    "..INTEL " */
    0x54,0x45,0x4D,0x50,0x4C,0x41,0x54,0x45,  /* 00000010    "TEMPLATE" */
    0x01,0x00,0x00,0x00,0x49,0x4E,0x54,0x4C,  /* 00000018    "....INTL" */
    0x15,0x11,0x13,0x20,0x01,0x00,0x00,0x00   /* 00000020    "... ...." */
};

unsigned char XsdtCode[] =
{
    0x58,0x53,0x44,0x54,0x2C,0x00,0x00,0x00,  /* 00000000    "XSDT,..." */
    0x01,0x06,0x49,0x4E,0x54,0x45,0x4C,0x20,  /* 00000008    "..INTEL " */
    0x54,0x45,0x4D,0x50,0x4C,0x41,0x54,0x45,  /* 00000010    "TEMPLATE" */
    0x01,0x00,0x00,0x00,0x49,0x4E,0x54,0x4C,  /* 00000018    "....INTL" */
    0x15,0x11,0x13,0x20,0x01,0x00,0x00,0x00,  /* 00000020    "... ...." */
    0x00,0x00,0x00,0x00                       /* 00000028    "...."     */
};

unsigned char FadtCode[] =
{
    0x46,0x41,0x43,0x50,0x0C,0x01,0x00,0x00,  /* 00000000    "FACP...." */
    0x05,0x64,0x49,0x4E,0x54,0x45,0x4C,0x20,  /* 00000008    ".dINTEL " */
    0x54,0x45,0x4D,0x50,0x4C,0x41,0x54,0x45,  /* 00000010    "TEMPLATE" */
    0x00,0x00,0x00,0x00,0x49,0x4E,0x54,0x4C,  /* 00000018    "....INTL" */
    0x15,0x11,0x13,0x20,0x01,0x00,0x00,0x00,  /* 00000020    "... ...." */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000028    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000030    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000038    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000040    "........" */
    0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,  /* 00000048    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000050    "........" */
    0x04,0x02,0x01,0x04,0x08,0x00,0x00,0x00,  /* 00000058    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000060    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000068    "........" */
    0x00,0x00,0x00,0x00,0x01,0x08,0x00,0x01,  /* 00000070    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000078    "........" */
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,  /* 00000080    "........" */
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,  /* 00000088    "........" */
    0x00,0x00,0x00,0x00,0x01,0x20,0x00,0x02,  /* 00000090    "..... .." */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000098    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 000000A0    "........" */
    0x00,0x00,0x00,0x00,0x01,0x10,0x00,0x02,  /* 000000A8    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 000000B0    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 000000B8    "........" */
    0x00,0x00,0x00,0x00,0x01,0x08,0x00,0x00,  /* 000000C0    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 000000C8    "........" */
    0x01,0x20,0x00,0x03,0x01,0x00,0x00,0x00,  /* 000000D0    ". ......" */
    0x00,0x00,0x00,0x00,0x01,0x40,0x00,0x01,  /* 000000D8    ".....@.." */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 000000E0    "........" */
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 000000E8    "........" */
    0x00,0x00,0x00,0x00,0x01,0x08,0x00,0x01,  /* 000000F0    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 000000F8    "........" */
    0x01,0x08,0x00,0x01,0x00,0x00,0x00,0x00,  /* 00000100    "........" */
    0x00,0x00,0x00,0x00                       /* 00000108    "...."     */
};

/* Fixed tables */

static unsigned char FacsCode[] =
{
    0x46,0x41,0x43,0x53,0x40,0x00,0x00,0x00,  /* 00000000    "FACS@..." */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000008    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000010    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000018    "........" */
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000020    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000028    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00000030    "........" */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00   /* 00000038    "........" */
};

static unsigned char DsdtCode[] =
{
    0x44,0x53,0x44,0x54,0x8C,0x00,0x00,0x00,  /* 00000000    "DSDT...." */
    0x02,0x76,0x49,0x6E,0x74,0x65,0x6C,0x00,  /* 00000008    ".vIntel." */
    0x54,0x65,0x6D,0x70,0x6C,0x61,0x74,0x65,  /* 00000010    "Template" */
    0x01,0x00,0x00,0x00,0x49,0x4E,0x54,0x4C,  /* 00000018    "....INTL" */
    0x24,0x04,0x14,0x20,0x5B,0x80,0x47,0x4E,  /* 00000020    "$.. [.GN" */
    0x56,0x53,0x00,0x0C,0x98,0xEE,0xBB,0xDF,  /* 00000028    "VS......" */
    0x0A,0x13,0x5B,0x81,0x0B,0x47,0x4E,0x56,  /* 00000030    "..[..GNV" */
    0x53,0x00,0x46,0x4C,0x44,0x31,0x08,0x14,  /* 00000038    "S.FLD1.." */
    0x4C,0x04,0x4D,0x41,0x49,0x4E,0x01,0x70,  /* 00000040    "L.MAIN.p" */
    0x73,0x0D,0x4D,0x61,0x69,0x6E,0x2F,0x41,  /* 00000048    "s.Main/A" */
    0x72,0x67,0x30,0x3A,0x20,0x00,0x68,0x00,  /* 00000050    "rg0: .h." */
    0x5B,0x31,0x70,0x00,0x46,0x4C,0x44,0x31,  /* 00000058    "[1p.FLD1" */
    0x86,0x5C,0x00,0x00,0xA4,0x0D,0x4D,0x61,  /* 00000060    ".\....Ma" */
    0x69,0x6E,0x20,0x73,0x75,0x63,0x63,0x65,  /* 00000068    "in succe" */
    0x73,0x73,0x66,0x75,0x6C,0x6C,0x79,0x20,  /* 00000070    "ssfully " */
    0x63,0x6F,0x6D,0x70,0x6C,0x65,0x74,0x65,  /* 00000078    "complete" */
    0x64,0x20,0x65,0x78,0x65,0x63,0x75,0x74,  /* 00000080    "d execut" */
    0x69,0x6F,0x6E,0x00                       /* 00000088    "ion."     */
};


/* Useful pointers */

ACPI_TABLE_RSDP *Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, RsdpCode);
ACPI_TABLE_RSDT *Rsdt = ACPI_CAST_PTR (ACPI_TABLE_RSDT, RsdtCode);
ACPI_TABLE_XSDT *Xsdt = ACPI_CAST_PTR (ACPI_TABLE_XSDT, XsdtCode);
ACPI_TABLE_FADT *Fadt = ACPI_CAST_PTR (ACPI_TABLE_FADT, FadtCode);


/******************************************************************************
 *
 * Build the various required ACPI tables:
 *
 * 1) Setup RSDP to point to the RSDT and XSDT
 * 2) Setup RSDT/XSDT to point to the FADT
 * 3) Setup FADT to point to the DSDT and FACS
 * 4) Update checksums for all modified tables
 *
 *****************************************************************************/

void
ExInitializeAcpiTables (
    void)
{

    /* Setup RSDP */

    Rsdp->RsdtPhysicalAddress = (UINT32) ACPI_TO_INTEGER (RsdtCode);
    Rsdp->XsdtPhysicalAddress = (UINT64) ACPI_TO_INTEGER (XsdtCode);

    /* RSDT and XSDT */

    Rsdt->TableOffsetEntry[0] = (UINT32) ACPI_TO_INTEGER (FadtCode);
    Xsdt->TableOffsetEntry[0] = (UINT64) ACPI_TO_INTEGER (FadtCode);

    /* FADT */

    Fadt->Facs = 0;
    Fadt->Dsdt = 0;
    Fadt->XFacs = (UINT64) ACPI_TO_INTEGER (FacsCode);
    Fadt->XDsdt = (UINT64) ACPI_TO_INTEGER (DsdtCode);

    /* Set new checksums for the modified tables */

    Rsdp->Checksum = 0;
    Rsdp->Checksum = (UINT8) -AcpiTbChecksum (
        (void *) RsdpCode, ACPI_RSDP_CHECKSUM_LENGTH);

    Rsdt->Header.Checksum = 0;
    Rsdt->Header.Checksum = (UINT8) -AcpiTbChecksum (
        (void *) Rsdt, Rsdt->Header.Length);

    Xsdt->Header.Checksum = 0;
    Xsdt->Header.Checksum =  (UINT8) -AcpiTbChecksum (
        (void *) Xsdt, Xsdt->Header.Length);

    Fadt->Header.Checksum = 0;
    Fadt->Header.Checksum =  (UINT8) -AcpiTbChecksum (
        (void *) Fadt, Fadt->Header.Length);
}


/******************************************************************************
 *
 * OSL support - return the address of the RSDP
 *
 *****************************************************************************/

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer (
    void)
{

    return (ACPI_PTR_TO_PHYSADDR (RsdpCode));
}


#ifdef DO_NOT_COMPILE_ACPI_TABLE_CODE
/******************************************************************************
 *
 * ACPICA Example table source code
 *
 * This is the original source code for the tables above
 *
 *****************************************************************************/

/* RSDP */

[0008]                          Signature : "RSD PTR "
[0001]                           Checksum : 43
[0006]                             Oem ID : "INTEL "
[0001]                           Revision : 02
[0004]                       RSDT Address : 00000000
[0004]                             Length : 00000024
[0008]                       XSDT Address : 0000000000000000
[0001]                  Extended Checksum : DC
[0003]                           Reserved : 000000


/* RSDT */

[0004]                          Signature : "RSDT"    [Root System Description Table]
[0004]                       Table Length : 00000044
[0001]                           Revision : 01
[0001]                           Checksum : B1
[0006]                             Oem ID : "INTEL "
[0008]                       Oem Table ID : "TEMPLATE"
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20100528

[0004]             ACPI Table Address   0 : 00000001


/* XSDT */

[0004]                          Signature : "XSDT"    [Extended System Description Table]
[0004]                       Table Length : 00000064
[0001]                           Revision : 01
[0001]                           Checksum : 8B
[0006]                             Oem ID : "INTEL "
[0008]                       Oem Table ID : "TEMPLATE"
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20100528

[0008]             ACPI Table Address   0 : 0000000000000001


/* FADT */

[0004]                          Signature : "FACP"    [Fixed ACPI Description Table (FADT)]
[0004]                       Table Length : 0000010C
[0001]                           Revision : 05
[0001]                           Checksum : 18
[0006]                             Oem ID : "INTEL "
[0008]                       Oem Table ID : "TEMPLATE"
[0004]                       Oem Revision : 00000000
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20111123

[0004]                       FACS Address : 00000001
[0004]                       DSDT Address : 00000001
[0001]                              Model : 00
[0001]                         PM Profile : 00 [Unspecified]
[0002]                      SCI Interrupt : 0000
[0004]                   SMI Command Port : 00000000
[0001]                  ACPI Enable Value : 00
[0001]                 ACPI Disable Value : 00
[0001]                     S4BIOS Command : 00
[0001]                    P-State Control : 00
[0004]           PM1A Event Block Address : 00000001
[0004]           PM1B Event Block Address : 00000000
[0004]         PM1A Control Block Address : 00000001
[0004]         PM1B Control Block Address : 00000000
[0004]          PM2 Control Block Address : 00000001
[0004]             PM Timer Block Address : 00000001
[0004]                 GPE0 Block Address : 00000001
[0004]                 GPE1 Block Address : 00000000
[0001]             PM1 Event Block Length : 04
[0001]           PM1 Control Block Length : 02
[0001]           PM2 Control Block Length : 01
[0001]              PM Timer Block Length : 04
[0001]                  GPE0 Block Length : 08
[0001]                  GPE1 Block Length : 00
[0001]                   GPE1 Base Offset : 00
[0001]                       _CST Support : 00
[0002]                         C2 Latency : 0000
[0002]                         C3 Latency : 0000
[0002]                     CPU Cache Size : 0000
[0002]                 Cache Flush Stride : 0000
[0001]                  Duty Cycle Offset : 00
[0001]                   Duty Cycle Width : 00
[0001]                RTC Day Alarm Index : 00
[0001]              RTC Month Alarm Index : 00
[0001]                  RTC Century Index : 00
[0002]         Boot Flags (decoded below) : 0000
            Legacy Devices Supported (V2) : 0
         8042 Present on ports 60/64 (V2) : 0
                     VGA Not Present (V4) : 0
                   MSI Not Supported (V4) : 0
             PCIe ASPM Not Supported (V4) : 0
                CMOS RTC Not Present (V5) : 0
[0001]                           Reserved : 00
[0004]              Flags (decoded below) : 00000000
   WBINVD instruction is operational (V1) : 0
           WBINVD flushes all caches (V1) : 0
                 All CPUs support C1 (V1) : 0
               C2 works on MP system (V1) : 0
         Control Method Power Button (V1) : 0
         Control Method Sleep Button (V1) : 0
     RTC wake not in fixed reg space (V1) : 0
         RTC can wake system from S4 (V1) : 0
                     32-bit PM Timer (V1) : 0
                   Docking Supported (V1) : 0
            Reset Register Supported (V2) : 0
                         Sealed Case (V3) : 0
                 Headless - No Video (V3) : 0
     Use native instr after SLP_TYPx (V3) : 0
           PCIEXP_WAK Bits Supported (V4) : 0
                  Use Platform Timer (V4) : 0
            RTC_STS valid on S4 wake (V4) : 0
             Remote Power-on capable (V4) : 0
              Use APIC Cluster Model (V4) : 0
  Use APIC Physical Destination Mode (V4) : 0
                    Hardware Reduced (V5) : 0
                   Low Power S0 Idle (V5) : 0

[0012]                     Reset Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000001

[0001]               Value to cause reset : 00
[0003]                           Reserved : 000000
[0008]                       FACS Address : 0000000000000001
[0008]                       DSDT Address : 0000000000000001
[0012]                   PM1A Event Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 20
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 02 [Word Access:16]
[0008]                            Address : 0000000000000001

[0012]                   PM1B Event Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                 PM1A Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 10
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 02 [Word Access:16]
[0008]                            Address : 0000000000000001

[0012]                 PM1B Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                  PM2 Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000001

[0012]                     PM Timer Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 20
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 03 [DWord Access:32]
[0008]                            Address : 0000000000000001

[0012]                         GPE0 Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 40
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000001

[0012]                         GPE1 Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000


[0012]             Sleep Control Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000000

[0012]              Sleep Status Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 01 [Byte Access:8]
[0008]                            Address : 0000000000000000


/* FACS */

[0004]                          Signature : "FACS"
[0004]                             Length : 00000040
[0004]                 Hardware Signature : 00000000
[0004]          32 Firmware Waking Vector : 00000000
[0004]                        Global Lock : 00000000
[0004]              Flags (decoded below) : 00000000
                   S4BIOS Support Present : 0
               64-bit Wake Supported (V2) : 0
[0008]          64 Firmware Waking Vector : 0000000000000000
[0001]                            Version : 02
[0003]                           Reserved : 000000
[0004]          OspmFlags (decoded below) : 00000000
            64-bit Wake Env Required (V2) : 0


/* DSDT - ASL code */

DefinitionBlock ("dsdt.aml", "DSDT", 2, "Intel", "Template", 0x00000001)
{
    OperationRegion (GNVS, SystemMemory, 0xDFBBEE98, 0x00000013)
    Field (GNVS, AnyAcc, NoLock, Preserve)
    {
        FLD1,   8,
    }

    Method (MAIN, 1, NotSerialized)
    {
        Store (Concatenate ("Main/Arg0: ", Arg0), Debug)
        Store (Zero, FLD1)
        Notify (\, Zero)
        Return ("Main successfully completed execution")
    }
}
#endif
