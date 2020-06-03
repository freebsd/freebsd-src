/** @file
  DMA Remapping Reporting (DMAR) ACPI table definition from Intel(R)
  Virtualization Technology for Directed I/O (VT-D) Architecture Specification.

  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - Intel(R) Virtualization Technology for Directed I/O (VT-D) Architecture
      Specification v2.5, Dated November 2017.
      http://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/vt-directed-io-spec.pdf

  @par Glossary:
    - HPET - High Precision Event Timer
    - NUMA - Non-uniform Memory Access
**/
#ifndef _DMA_REMAPPING_REPORTING_TABLE_H_
#define _DMA_REMAPPING_REPORTING_TABLE_H_

#include <IndustryStandard/Acpi.h>

#pragma pack(1)

///
/// DMA-Remapping Reporting Structure definitions from section 8.1
///@{
#define EFI_ACPI_DMAR_REVISION                             0x01

#define EFI_ACPI_DMAR_FLAGS_INTR_REMAP                     BIT0
#define EFI_ACPI_DMAR_FLAGS_X2APIC_OPT_OUT                 BIT1
#define EFI_ACPI_DMAR_FLAGS_DMA_CTRL_PLATFORM_OPT_IN_FLAG  BIT2
///@}

///
/// Remapping Structure Types definitions from section 8.2
///@{
#define EFI_ACPI_DMAR_TYPE_DRHD                   0x00
#define EFI_ACPI_DMAR_TYPE_RMRR                   0x01
#define EFI_ACPI_DMAR_TYPE_ATSR                   0x02
#define EFI_ACPI_DMAR_TYPE_RHSA                   0x03
#define EFI_ACPI_DMAR_TYPE_ANDD                   0x04
///@}

///
/// DMA-Remapping Hardware Unit definitions from section 8.3
///
#define EFI_ACPI_DMAR_DRHD_FLAGS_INCLUDE_PCI_ALL  BIT0

///
/// DMA-Remapping Device Scope Entry Structure definitions from section 8.3.1
///@{
#define EFI_ACPI_DEVICE_SCOPE_ENTRY_TYPE_PCI_ENDPOINT           0x01
#define EFI_ACPI_DEVICE_SCOPE_ENTRY_TYPE_PCI_BRIDGE             0x02
#define EFI_ACPI_DEVICE_SCOPE_ENTRY_TYPE_IOAPIC                 0x03
#define EFI_ACPI_DEVICE_SCOPE_ENTRY_TYPE_MSI_CAPABLE_HPET       0x04
#define EFI_ACPI_DEVICE_SCOPE_ENTRY_TYPE_ACPI_NAMESPACE_DEVICE  0x05
///@}

///
/// Root Port ATS Capability Reporting Structure definitions from section 8.5
///
#define EFI_ACPI_DMAR_ATSR_FLAGS_ALL_PORTS                      BIT0

///
/// Definition for DMA Remapping Structure Header
///
typedef struct {
  UINT16        Type;
  UINT16        Length;
} EFI_ACPI_DMAR_STRUCTURE_HEADER;

///
/// Definition for DMA-Remapping PCI Path
///
typedef struct {
  UINT8         Device;
  UINT8         Function;
} EFI_ACPI_DMAR_PCI_PATH;

///
/// Device Scope Structure is defined in section 8.3.1
///
typedef struct {
  UINT8         Type;
  UINT8         Length;
  UINT16        Reserved2;
  UINT8         EnumerationId;
  UINT8         StartBusNumber;
} EFI_ACPI_DMAR_DEVICE_SCOPE_STRUCTURE_HEADER;

/**
  DMA-remapping hardware unit definition (DRHD) structure is defined in
  section 8.3. This uniquely represents a remapping hardware unit present
  in the platform. There must be at least one instance of this structure
  for each PCI segment in the platform.
**/
typedef struct {
  EFI_ACPI_DMAR_STRUCTURE_HEADER  Header;
  /**
    - Bit[0]: INCLUDE_PCI_ALL
              - If Set, this remapping hardware unit has under its scope all
                PCI compatible devices in the specified Segment, except devices
                reported under the scope of other remapping hardware units for
                the same Segment.
              - If Clear, this remapping hardware unit has under its scope only
                devices in the specified Segment that are explicitly identified
                through the DeviceScope field.
    - Bits[7:1] Reserved.
  **/
  UINT8                           Flags;
  UINT8                           Reserved;
  ///
  /// The PCI Segment associated with this unit.
  ///
  UINT16                          SegmentNumber;
  ///
  /// Base address of remapping hardware register-set for this unit.
  ///
  UINT64                          RegisterBaseAddress;
} EFI_ACPI_DMAR_DRHD_HEADER;

/**
  Reserved Memory Region Reporting Structure (RMRR) is described in section 8.4
  Reserved memory ranges that may be DMA targets may be reported through the
  RMRR structures, along with the devices that requires access to the specified
  reserved memory region.
**/
typedef struct {
  EFI_ACPI_DMAR_STRUCTURE_HEADER  Header;
  UINT8                           Reserved[2];
  ///
  /// PCI Segment Number associated with devices identified through
  /// the Device Scope field.
  ///
  UINT16                          SegmentNumber;
  ///
  /// Base address of 4KB-aligned reserved memory region
  ///
  UINT64                          ReservedMemoryRegionBaseAddress;
  /**
    Last address of the reserved memory region. Value in this field must be
    greater than the value in Reserved Memory Region Base Address field.
    The reserved memory region size (Limit - Base + 1) must be an integer
    multiple of 4KB.
  **/
  UINT64                          ReservedMemoryRegionLimitAddress;
} EFI_ACPI_DMAR_RMRR_HEADER;

/**
  Root Port ATS Capability Reporting (ATSR) structure is defined in section 8.5.
  This structure is applicable only for platforms supporting Device-TLBs as
  reported through the Extended Capability Register. For each PCI Segment in
  the platform that supports Device-TLBs, BIOS provides an ATSR structure. The
  ATSR structures identifies PCI-Express Root-Ports supporting Address
  Translation Services (ATS) transactions. Software must enable ATS on endpoint
  devices behind a Root Port only if the Root Port is reported as supporting
  ATS transactions.
**/
typedef struct {
  EFI_ACPI_DMAR_STRUCTURE_HEADER  Header;
  /**
    - Bit[0]: ALL_PORTS:
              - If Set, indicates all PCI Express Root Ports in the specified
                PCI Segment supports ATS transactions.
              - If Clear, indicates ATS transactions are supported only on
                Root Ports identified through the Device Scope field.
    - Bits[7:1] Reserved.
  **/
  UINT8                           Flags;
  UINT8                           Reserved;
  ///
  /// The PCI Segment associated with this ATSR structure
  ///
  UINT16                          SegmentNumber;
} EFI_ACPI_DMAR_ATSR_HEADER;

/**
  Remapping Hardware Static Affinity (RHSA) is an optional structure defined
  in section 8.6. This is intended to be used only on NUMA platforms with
  Remapping hardware units and memory spanned across multiple nodes.
  When used, there must be a RHSA structure for each Remapping hardware unit
  reported through DRHD structure.
**/
typedef struct {
  EFI_ACPI_DMAR_STRUCTURE_HEADER  Header;
  UINT8                           Reserved[4];
  ///
  /// Register Base Address of this Remap hardware unit reported in the
  /// corresponding DRHD structure.
  ///
  UINT64                          RegisterBaseAddress;
  ///
  /// Proximity Domain to which the Remap hardware unit identified by the
  /// Register Base Address field belongs.
  ///
  UINT32                          ProximityDomain;
} EFI_ACPI_DMAR_RHSA_HEADER;

/**
  An ACPI Name-space Device Declaration (ANDD) structure is defined in section
  8.7 and uniquely represents an ACPI name-space enumerated device capable of
  issuing DMA requests in the platform. ANDD structures are used in conjunction
  with Device-Scope entries of type ACPI_NAMESPACE_DEVICE.
**/
typedef struct {
  EFI_ACPI_DMAR_STRUCTURE_HEADER  Header;
  UINT8                           Reserved[3];
  /**
    Each ACPI device enumerated through an ANDD structure must have a unique
    value for this field. To report an ACPI device with ACPI Device Number
    value of X, under the scope of a DRHD unit, a Device-Scope entry of type
    ACPI_NAMESPACE_DEVICE is used with value of X in the Enumeration ID field.
    The Start Bus Number and Path fields in the Device-Scope together
    provides the 16-bit source-id allocated by platform for the ACPI device.
  **/
  UINT8                           AcpiDeviceNumber;
} EFI_ACPI_DMAR_ANDD_HEADER;

/**
  DMA Remapping Reporting Structure Header as defined in section 8.1
  This header will be followed by list of Remapping Structures listed below
    - DMA Remapping Hardware Unit Definition (DRHD)
    - Reserved Memory Region Reporting (RMRR)
    - Root Port ATS Capability Reporting (ATSR)
    - Remapping Hardware Static Affinity (RHSA)
    - ACPI Name-space Device Declaration (ANDD)
  These structure types must by reported in numerical order.
  i.e., All remapping structures of type 0 (DRHD) enumerated before remapping
  structures of type 1 (RMRR), and so forth.
**/
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER     Header;
  /**
    This field indicates the maximum DMA physical addressability supported by
    this platform. The system address map reported by the BIOS indicates what
    portions of this addresses are populated. The Host Address Width (HAW) of
    the platform is computed as (N+1), where N is the value reported in this
    field.
    For example, for a platform supporting 40 bits of physical addressability,
    the value of 100111b is reported in this field.
  **/
  UINT8                           HostAddressWidth;
  /**
    - Bit[0]:   INTR_REMAP - If Clear, the platform does not support interrupt
                remapping. If Set, the platform supports interrupt remapping.
    - Bit[1]:   X2APIC_OPT_OUT - For firmware compatibility reasons, platform
                firmware may Set this field to request system software to opt
                out of enabling Extended xAPIC (X2APIC) mode. This field is
                valid only when the INTR_REMAP field (bit 0) is Set.
    - Bit[2]:   DMA_CTRL_PLATFORM_OPT_IN_FLAG - Platform firmware is
                recommended to Set this field to report any platform initiated
                DMA is restricted to only reserved memory regions (reported in
                RMRR structures) when transferring control to system software
                such as on ExitBootServices().
    - Bits[7:3] Reserved.
  **/
  UINT8                           Flags;
  UINT8                           Reserved[10];
} EFI_ACPI_DMAR_HEADER;

#pragma pack()

#endif
