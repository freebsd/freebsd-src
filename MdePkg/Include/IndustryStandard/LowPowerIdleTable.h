/** @file
  ACPI Low Power Idle Table (LPIT) definitions

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - ACPI Low Power Idle Table (LPIT) Revision 001, dated July 2014
      http://www.uefi.org/sites/default/files/resources/ACPI_Low_Power_Idle_Table.pdf

  @par Glossary:
    - GAS - Generic Address Structure
    - LPI - Low Power Idle
**/

#ifndef _LOW_POWER_IDLE_TABLE_H_
#define _LOW_POWER_IDLE_TABLE_H_

#include <IndustryStandard/Acpi.h>

#pragma pack(1)

///
/// LPI Structure Types
///
#define ACPI_LPI_STRUCTURE_TYPE_NATIVE_CSTATE  0x00

///
/// Low Power Idle (LPI) State Flags
///
typedef union {
  struct {
    UINT32    Disabled           : 1; ///< If set, LPI state is not used

    /**
      If set, Residency counter is not available for this LPI state and
      Residency Counter Frequency is invalid
    **/
    UINT32    CounterUnavailable : 1;
    UINT32    Reserved           : 30; ///< Reserved for future use. Must be zero
  } Bits;
  UINT32    Data32;
} ACPI_LPI_STATE_FLAGS;

///
/// Low Power Idle (LPI) structure with Native C-state instruction entry trigger descriptor
///
typedef struct {
  UINT32                                    Type;   ///< LPI State descriptor Type 0
  UINT32                                    Length; ///< Length of LPI state Descriptor Structure
  ///
  /// Unique LPI state identifier: zero based, monotonically increasing identifier
  ///
  UINT16                                    UniqueId;
  UINT8                                     Reserved[2]; ///< Must be Zero
  ACPI_LPI_STATE_FLAGS                      Flags;       ///< LPI state flags

  /**
    The LPI entry trigger, matching an existing _CST.Register object, represented as a
    Generic Address Structure. All processors must request this state or deeper to trigger.
  **/
  EFI_ACPI_6_1_GENERIC_ADDRESS_STRUCTURE    EntryTrigger;
  UINT32                                    Residency; ///< Minimum residency or break-even in uSec
  UINT32                                    Latency;   ///< Worst case exit latency in uSec

  /**
    [optional] Residency counter, represented as a Generic Address Structure.
    If not present, Flags[1] bit should be set.
  **/
  EFI_ACPI_6_1_GENERIC_ADDRESS_STRUCTURE    ResidencyCounter;

  /**
    [optional] Residency counter frequency in cycles per second. Value 0 indicates that
    counter runs at TSC frequency. Valid only if Residency Counter is present.
  **/
  UINT64                                    ResidencyCounterFrequency;
} ACPI_LPI_NATIVE_CSTATE_DESCRIPTOR;

#pragma pack()

#endif
