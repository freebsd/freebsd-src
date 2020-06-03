/** @file
  ACPI high precision event timer table definition, at www.intel.com
  Specification name is IA-PC HPET (High Precision Event Timers) Specification.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _HIGH_PRECISION_EVENT_TIMER_TABLE_H_
#define _HIGH_PRECISION_EVENT_TIMER_TABLE_H_

#include <IndustryStandard/Acpi.h>

//
// Ensure proper structure formats
//
#pragma pack(1)

///
/// HPET Event Timer Block ID described in IA-PC HPET Specification, 3.2.4.
///
typedef union {
  struct {
    UINT32 Revision       : 8;
    UINT32 NumberOfTimers : 5;
    UINT32 CounterSize    : 1;
    UINT32 Reserved       : 1;
    UINT32 LegacyRoute    : 1;
    UINT32 VendorId       : 16;
  }      Bits;
  UINT32 Uint32;
} EFI_ACPI_HIGH_PRECISION_EVENT_TIMER_BLOCK_ID;


///
/// High Precision Event Timer Table header definition.
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER             Header;
  UINT32                                  EventTimerBlockId;
  EFI_ACPI_2_0_GENERIC_ADDRESS_STRUCTURE  BaseAddressLower32Bit;
  UINT8                                   HpetNumber;
  UINT16                                  MainCounterMinimumClockTickInPeriodicMode;
  UINT8                                   PageProtectionAndOemAttribute;
} EFI_ACPI_HIGH_PRECISION_EVENT_TIMER_TABLE_HEADER;

///
/// HPET Revision (defined in spec)
///
#define EFI_ACPI_HIGH_PRECISION_EVENT_TIMER_TABLE_REVISION  0x01

//
// Page protection setting
// Values 3 through 15 are reserved for use by the specification
//
#define EFI_ACPI_NO_PAGE_PROTECTION   0
#define EFI_ACPI_4KB_PAGE_PROTECTION  1
#define EFI_ACPI_64KB_PAGE_PROTECTION 2

#pragma pack()

#endif
