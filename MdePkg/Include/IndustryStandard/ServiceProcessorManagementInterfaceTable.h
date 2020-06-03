/** @file
  Service Processor Management Interface (SPMI) ACPI table definition from
  Intelligent Platform Management Interface Specification Second Generation.

  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - Intelligent Platform Management Interface Specification Second Generation
      v2.0 Revision 1.1, Dated October 2013.
      https://www.intel.com/content/dam/www/public/us/en/documents/specification-updates/ipmi-intelligent-platform-mgt-interface-spec-2nd-gen-v2-0-spec-update.pdf
**/
#ifndef _SERVICE_PROCESSOR_MANAGEMENT_INTERFACE_TABLE_H_
#define _SERVICE_PROCESSOR_MANAGEMENT_INTERFACE_TABLE_H_

#include <IndustryStandard/Acpi.h>

#pragma pack(1)

///
/// Definition for the device identification information used by the Service
/// Processor Management Interface Description Table
///
typedef union {
  ///
  /// For PCI IPMI device
  ///
  struct {
    UINT8                                 SegmentGroup;
    UINT8                                 Bus;
    UINT8                                 Device;
    UINT8                                 Function;
  } Pci;
  ///
  /// For non-PCI IPMI device, the ACPI _UID value of the device
  ///
  UINT32                                  Uid;
} EFI_ACPI_SERVICE_PROCESSOR_MANAGEMENT_INTERFACE_TABLE_DEVICE_ID;


///
/// Definition for Service Processor Management Interface Description Table
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER               Header;
  ///
  /// Indicates the type of IPMI interface.
  ///
  UINT8                                     InterfaceType;
  ///
  /// This field must always be 01h to be compatible with any software that
  /// implements previous versions of this spec.
  ///
  UINT8                                     Reserved1;
  ///
  /// Identifies the IPMI specification revision, in BCD format.
  ///
  UINT16                                    SpecificationRevision;
  ///
  /// Interrupt type(s) used by the interface.
  ///
  UINT8                                     InterruptType;
  ///
  /// The bit assignment of the SCI interrupt within the GPEx_STS register of a
  /// GPE described if the FADT that the interface triggers.
  ///
  UINT8                                     Gpe;
  ///
  /// Reserved, must be 00h.
  ///
  UINT8                                     Reserved2;
  ///
  /// PCI Device Flag.
  ///
  UINT8                                     PciDeviceFlag;
  ///
  /// The I/O APIC or I/O SAPIC Global System Interrupt used by the interface.
  ///
  UINT32                                    GlobalSystemInterrupt;
  ///
  /// The base address of the interface register set described using the
  /// Generic Address Structure (GAS, See [ACPI 2.0] for the definition).
  ///
  EFI_ACPI_2_0_GENERIC_ADDRESS_STRUCTURE    BaseAddress;
  ///
  /// Device identification information.
  ///
  EFI_ACPI_SERVICE_PROCESSOR_MANAGEMENT_INTERFACE_TABLE_DEVICE_ID    DeviceId;
  ///
  /// This field must always be null (0x00) to be compatible with any software
  /// that implements previous versions of this spec.
  ///
  UINT8                                     Reserved3;
} EFI_ACPI_SERVICE_PROCESSOR_MANAGEMENT_INTERFACE_TABLE;

#pragma pack()

#endif
