/** @file
  ACPI for Memory System Resource Partitioning and Monitoring 2.0 (MPAM) as
  specified in ARM spec DEN0065

  Copyright (c) 2024, Arm Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
   - [1] ACPI for Memory System Resource Partitioning and Monitoring 2.0
     (https://developer.arm.com/documentation/den0065/latest)

  @par Glossary:
    - MPAM - Memory System Resource Partitioning And Monitoring
    - MSC  - Memory System Component
    - PCC  - Platform Communication Channel
    - RIS  - Resource Instance Selection
    - SMMU - Arm System Memory Management Unit
 **/

#ifndef MPAM_H_
#define MPAM_H_

#include <IndustryStandard/Acpi.h>

///
/// MPAM Revision
///
#define EFI_ACPI_MEMORY_SYSTEM_RESOURCE_PARTITIONING_AND_MONITORING_TABLE_REVISION  (0x01)

///
/// MPAM Interrupt mode
///
#define EFI_ACPI_MPAM_INTERRUPT_LEVEL_TRIGGERED  (0x0)
#define EFI_ACPI_MPAM_INTERRUPT_EDGE_TRIGGERED   (0x1)

///
/// MPAM Interrupt type
///
#define EFI_ACPI_MPAM_INTERRUPT_WIRED  (0x0)

///
/// MPAM Interrupt affinity type
///
#define EFI_ACPI_MPAM_INTERRUPT_PROCESSOR_AFFINITY            (0x0)
#define EFI_ACPI_MPAM_INTERRUPT_PROCESSOR_CONTAINER_AFFINITY  (0x1)

///
/// MPAM MSC affinity valid
///
#define EFI_ACPI_MPAM_INTERRUPT_AFFINITY_NOT_VALID  (0x0)
#define EFI_ACPI_MPAM_INTERRUPT_AFFINITY_VALID      (0x1)

///
/// MPAM Interrupt flag - bit positions
///
#define EFI_ACPI_MPAM_INTERRUPT_MODE_SHIFT            (0)
#define EFI_ACPI_MPAM_INTERRUPT_TYPE_SHIFT            (1)
#define EFI_ACPI_MPAM_INTERRUPT_AFFINITY_TYPE_SHIFT   (3)
#define EFI_ACPI_MPAM_INTERRUPT_AFFINITY_VALID_SHIFT  (4)
#define EFI_ACPI_MPAM_INTERRUPT_RESERVED_SHIFT        (5)

///
/// MPAM Interrupt flag - bit masks
///
#define EFI_ACPI_MPAM_INTERRUPT_MODE_MASK            (0x1)
#define EFI_ACPI_MPAM_INTERRUPT_TYPE_MASK            (0x3)
#define EFI_ACPI_MPAM_INTERRUPT_AFFINITY_TYPE_MASK   (0x8)
#define EFI_ACPI_MPAM_INTERRUPT_AFFINITY_VALID_MASK  (0x10)
#define EFI_ACPI_MPAM_INTERRUPT_RESERVED_MASK        (0xFFFFFFE0)

///
/// MPAM Location types
/// as described in document [1], table 11.
///
#define EFI_ACPI_MPAM_LOCATION_PROCESSOR_CACHE  (0x0)
#define EFI_ACPI_MPAM_LOCATION_MEMORY           (0x1)
#define EFI_ACPI_MPAM_LOCATION_SMMU             (0x2)
#define EFI_ACPI_MPAM_LOCATION_MEMORY_CACHE     (0x3)
#define EFI_ACPI_MPAM_LOCATION_ACPI_DEVICE      (0x4)
#define EFI_ACPI_MPAM_LOCATION_INTERCONNECT     (0x5)
#define EFI_ACPI_MPAM_LOCATION_UNKNOWN          (0xFF)

///
/// MPAM Interface types
/// as desscribed in document[1], table 4.
///
#define EFI_ACPI_MPAM_INTERFACE_MMIO  (0x00)
#define EFI_ACPI_MPAM_INTERFACE_PCC   (0x0A)

///
/// MPAM Link types
/// as described in document [1], table 19.
///
#define EFI_ACPI_MPAM_LINK_TYPE_NUMA  (0x00)
#define EFI_ACPI_MPAM_LINK_TYPE_PROC  (0x01)

#pragma pack(1)

///
/// MPAM MSC generic locator descriptor
/// as described in document [1], table 12.
///
typedef struct {
  UINT64    Descriptor1;
  UINT32    Descriptor2;
} EFI_ACPI_MPAM_GENERIC_LOCATOR;

///
/// MPAM processor cache locator descriptor
/// as described in document [1], table 13.
///
typedef struct {
  UINT64    CacheReference;
  UINT32    Reserved;
} EFI_ACPI_MPAM_CACHE_LOCATOR;

///
/// MPAM memory locator descriptor
/// as described in document [1], table 14.
///
typedef struct {
  UINT64    ProximityDomain;
  UINT32    Reserved;
} EFI_ACPI_MPAM_MEMORY_LOCATOR;

///
/// MPAM SMMU locator descriptor
/// as described in document [1], table 15.
///
typedef struct {
  UINT64    SmmuInterface;
  UINT32    Reserved;
} EFI_ACPI_MPAM_SMMU_LOCATOR;

///
/// MPAM memory-side cache locator descriptor
/// as described in Document [1], table 16.
///
typedef struct {
  UINT8     Reserved[7];
  UINT8     Level;
  UINT32    Reference;
} EFI_ACPI_MPAM_MEMORY_CACHE_LOCATOR;

///
/// MPAM ACPI device locator descriptor
/// as described in document [1], table 17.
///
typedef struct {
  UINT64    AcpiHardwareId;
  UINT32    AcpiUniqueId;
} EFI_ACPI_MPAM_ACPI_LOCATOR;

///
/// MPAM interconnect locator descriptor
/// as described in document [1], table 18.
///
typedef struct {
  UINT64    InterconnectDescTblOff;
  UINT32    Reserved;
} EFI_ACPI_MPAM_INTERCONNECT_LOCATOR;

///
/// MPAM interconnect descriptor
/// as described in document [1], table 19.
///
typedef struct {
  UINT32    SourceId;
  UINT32    DestinationId;
  UINT8     LinkType;
  UINT8     Reserved[3];
} EFI_ACPI_MPAM_INTERCONNECT_DESCRIPTOR;

///
/// MPAM interconnect descriptor table
/// as described in document [1], table 20.
///
typedef struct {
  UINT8     Signature[16];
  UINT32    NumDescriptors;
} EFI_ACPI_MPAM_INTERCONNECT_DESCRIPTOR_TABLE;

///
/// MPAM resource locator
///
typedef union {
  EFI_ACPI_MPAM_CACHE_LOCATOR           CacheLocator;
  EFI_ACPI_MPAM_MEMORY_LOCATOR          MemoryLocator;
  EFI_ACPI_MPAM_SMMU_LOCATOR            SmmuLocator;
  EFI_ACPI_MPAM_MEMORY_CACHE_LOCATOR    MemCacheLocator;
  EFI_ACPI_MPAM_ACPI_LOCATOR            AcpiLocator;
  EFI_ACPI_MPAM_INTERCONNECT_LOCATOR    InterconnectIfcLocator;
  EFI_ACPI_MPAM_GENERIC_LOCATOR         GenericLocator;
} EFI_ACPI_MPAM_LOCATOR;

///
/// MPAM MSC node body
/// as described document [1], table 4.
///
typedef struct {
  UINT16    Length;
  UINT8     InterfaceType;
  UINT8     Reserved;
  UINT32    Identifier;
  UINT64    BaseAddress;
  UINT32    MmioSize;
  UINT32    OverflowInterrupt;
  UINT32    OverflowInterruptFlags;
  UINT32    Reserved1;
  UINT32    OverflowInterruptAffinity;
  UINT32    ErrorInterrupt;
  UINT32    ErrorInterruptFlags;
  UINT32    Reserved2;
  UINT32    ErrorInterruptAffinity;
  UINT32    MaxNrdyUsec;
  UINT64    HardwareIdLinkedDevice;
  UINT32    InstanceIdLinkedDevice;
  UINT32    NumResources;
} EFI_ACPI_MPAM_MSC_NODE;

///
/// MPAM MSC resource
/// as described in document [1], table 9.
///
typedef struct {
  UINT32                   Identifier;
  UINT8                    RisIndex;
  UINT16                   Reserved1;
  UINT8                    LocatorType;
  EFI_ACPI_MPAM_LOCATOR    Locator;
  UINT32                   NumFunctionalDependencies;
} EFI_ACPI_MPAM_MSC_RESOURCE;

///
/// MPAM Function dependency descriptor
/// as described in document [1], table 10.
///
typedef struct {
  UINT32    Producer;
  UINT32    Reserved;
} EFI_ACPI_MPAM_FUNCTIONAL_DEPENDENCY_DESCRIPTOR;

#pragma pack()

#endif
