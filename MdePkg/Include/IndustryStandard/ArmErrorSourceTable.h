/** @file
  Arm Error Source Table as described in the
  'ACPI for the Armv8 RAS Extensions 1.1' Specification.

  Copyright (c) 2020 Arm Limited.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Reference(s):
    - ACPI for the Armv8 RAS Extensions 1.1 Platform Design Document,
      dated 28 September 2020.
      (https://developer.arm.com/documentation/den0085/0101/)

  @par Glossary
    - Ref : Reference
    - Id  : Identifier
**/

#ifndef ARM_ERROR_SOURCE_TABLE_H_
#define ARM_ERROR_SOURCE_TABLE_H_

///
/// "AEST" Arm Error Source Table
///
#define EFI_ACPI_6_3_ARM_ERROR_SOURCE_TABLE_SIGNATURE  SIGNATURE_32('A', 'E', 'S', 'T')

#define EFI_ACPI_ARM_ERROR_SOURCE_TABLE_REVISION  1

#pragma pack(1)

///
/// Arm Error Source Table definition.
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    Header;
} EFI_ACPI_ARM_ERROR_SOURCE_TABLE;

///
/// AEST Node structure.
///
typedef struct {
  /// Node type:
  ///   0x00 - Processor error node
  ///   0x01 - Memory error node
  ///   0x02 - SMMU error node
  ///   0x03 - Vendor-defined error node
  ///   0x04 - GIC error node
  UINT8     Type;

  /// Length of structure in bytes.
  UINT16    Length;

  /// Reserved - Must be zero.
  UINT8     Reserved;

  /// Offset from the start of the node to node-specific data.
  UINT32    DataOffset;

  /// Offset from the start of the node to the node interface structure.
  UINT32    InterfaceOffset;

  /// Offset from the start of the node to node interrupt array.
  UINT32    InterruptArrayOffset;

  /// Number of entries in the interrupt array.
  UINT32    InterruptArrayCount;

  // Generic node data

  /// The timestamp frequency of the counter in Hz.
  UINT64    TimestampRate;

  /// Reserved - Must be zero.
  UINT64    Reserved1;

  /// The rate in Hz at which the Error Generation Counter decrements.
  UINT64    ErrorInjectionCountdownRate;
} EFI_ACPI_AEST_NODE_STRUCT;

// AEST Node type definitions
#define EFI_ACPI_AEST_NODE_TYPE_PROCESSOR       0x0
#define EFI_ACPI_AEST_NODE_TYPE_MEMORY          0x1
#define EFI_ACPI_AEST_NODE_TYPE_SMMU            0x2
#define EFI_ACPI_AEST_NODE_TYPE_VENDOR_DEFINED  0x3
#define EFI_ACPI_AEST_NODE_TYPE_GIC             0x4

///
/// AEST Node Interface structure.
///
typedef struct {
  /// Interface type:
  ///   0x0 - System register (SR)
  ///   0x1 - Memory mapped (MMIO)
  UINT8     Type;

  /// Reserved - Must be zero.
  UINT8     Reserved[3];

  /// AEST node interface flags.
  UINT32    Flags;

  /// Base address of error group that contains the error node.
  UINT64    BaseAddress;

  /// Zero-based index of the first standard error record that
  /// belongs to this node.
  UINT32    StartErrorRecordIndex;

  /// Number of error records in this node including both
  /// implemented and unimplemented records.
  UINT32    NumberErrorRecords;

  /// A bitmap indicating the error records within this
  /// node that are implemented in the current system.
  UINT64    ErrorRecordImplemented;

  /// A bitmap indicating the error records within this node that
  /// support error status reporting through the ERRGSR register.
  UINT64    ErrorRecordStatusReportingSupported;

  /// A bitmap indicating the addressing mode used by each error
  /// record within this node to populate the ERR<n>_ADDR register.
  UINT64    AddressingMode;
} EFI_ACPI_AEST_INTERFACE_STRUCT;

// AEST Interface node type definitions.
#define EFI_ACPI_AEST_INTERFACE_TYPE_SR    0x0
#define EFI_ACPI_AEST_INTERFACE_TYPE_MMIO  0x1

// AEST node interface flag definitions.
#define EFI_ACPI_AEST_INTERFACE_FLAG_PRIVATE      0
#define EFI_ACPI_AEST_INTERFACE_FLAG_SHARED       BIT0
#define EFI_ACPI_AEST_INTERFACE_FLAG_CLEAR_MISCX  BIT1

///
/// AEST Node Interrupt structure.
///
typedef struct {
  /// Interrupt type:
  ///   0x0 - Fault Handling Interrupt
  ///   0x1 - Error Recovery Interrupt
  UINT8     InterruptType;

  /// Reserved - Must be zero.
  UINT8     Reserved[2];

  /// Interrupt flags
  /// Bits [31:1]: Must be zero.
  /// Bit 0:
  ///   0b - Interrupt is edge-triggered
  ///   1b - Interrupt is level-triggered
  UINT8     InterruptFlags;

  /// GSIV of interrupt, if interrupt is an SPI or a PPI.
  UINT32    InterruptGsiv;

  /// If MSI is supported, then this field must be set to the
  /// Identifier field of the IORT ITS Group node.
  UINT8     ItsGroupRefId;

  /// Reserved - must be zero.
  UINT8     Reserved1[3];
} EFI_ACPI_AEST_INTERRUPT_STRUCT;

// AEST Interrupt node - interrupt type defintions.
#define EFI_ACPI_AEST_INTERRUPT_TYPE_FAULT_HANDLING  0x0
#define EFI_ACPI_AEST_INTERRUPT_TYPE_ERROR_RECOVERY  0x1

// AEST Interrupt node - interrupt flag defintions.
#define EFI_ACPI_AEST_INTERRUPT_FLAG_TRIGGER_TYPE_EDGE   0
#define EFI_ACPI_AEST_INTERRUPT_FLAG_TRIGGER_TYPE_LEVEL  BIT0

///
/// Cache Processor Resource structure.
///
typedef struct {
  /// Reference to the cache structure in the PPTT table.
  UINT32    CacheRefId;

  /// Reserved
  UINT32    Reserved;
} EFI_ACPI_AEST_PROCESSOR_CACHE_RESOURCE_STRUCT;

///
/// TLB Processor Resource structure.
///
typedef struct {
  /// TLB level from perspective of current processor.
  UINT32    TlbRefId;

  /// Reserved
  UINT32    Reserved;
} EFI_ACPI_AEST_PROCESSOR_TLB_RESOURCE_STRUCT;

///
/// Processor Generic Resource structure.
///
typedef struct {
  /// Vendor-defined supplementary data.
  UINT32    Data;
} EFI_ACPI_AEST_PROCESSOR_GENERIC_RESOURCE_STRUCT;

///
/// AEST Processor Resource union.
///
typedef union {
  /// Processor Cache resource.
  EFI_ACPI_AEST_PROCESSOR_CACHE_RESOURCE_STRUCT      Cache;

  /// Processor TLB resource.
  EFI_ACPI_AEST_PROCESSOR_TLB_RESOURCE_STRUCT        Tlb;

  /// Processor Generic resource.
  EFI_ACPI_AEST_PROCESSOR_GENERIC_RESOURCE_STRUCT    Generic;
} EFI_ACPI_AEST_PROCESSOR_RESOURCE;

///
/// AEST Processor structure.
///
typedef struct {
  /// AEST Node header
  EFI_ACPI_AEST_NODE_STRUCT           NodeHeader;

  /// Processor ID of node.
  UINT32                              AcpiProcessorId;

  /// Resource type of the processor node.
  ///   0x0 - Cache
  ///   0x1 - TLB
  ///   0x2 - Generic
  UINT8                               ResourceType;

  /// Reserved - must be zero.
  UINT8                               Reserved;

  /// Processor structure flags.
  UINT8                               Flags;

  /// Processor structure revision.
  UINT8                               Revision;

  /// Processor affinity descriptor for the resource that this
  /// error node pertains to.
  UINT64                              ProcessorAffinityLevelIndicator;

  /// Processor resource
  EFI_ACPI_AEST_PROCESSOR_RESOURCE    Resource;

  // Node Interface
  // EFI_ACPI_AEST_INTERFACE_STRUCT   NodeInterface;

  // Node Interrupt Array
  // EFI_ACPI_AEST_INTERRUPT_STRUCT   NodeInterruptArray[n];
} EFI_ACPI_AEST_PROCESSOR_STRUCT;

// AEST Processor resource type definitions.
#define EFI_ACPI_AEST_PROCESSOR_RESOURCE_TYPE_CACHE    0x0
#define EFI_ACPI_AEST_PROCESSOR_RESOURCE_TYPE_TLB      0x1
#define EFI_ACPI_AEST_PROCESSOR_RESOURCE_TYPE_GENERIC  0x2

// AEST Processor flag definitions.
#define EFI_ACPI_AEST_PROCESSOR_FLAG_GLOBAL  BIT0
#define EFI_ACPI_AEST_PROCESSOR_FLAG_SHARED  BIT1

///
/// Memory Controller structure.
///
typedef struct {
  /// AEST Node header
  EFI_ACPI_AEST_NODE_STRUCT    NodeHeader;

  /// SRAT proximity domain.
  UINT32                       ProximityDomain;

  // Node Interface
  // EFI_ACPI_AEST_INTERFACE_STRUCT   NodeInterface;

  // Node Interrupt Array
  // EFI_ACPI_AEST_INTERRUPT_STRUCT   NodeInterruptArray[n];
} EFI_ACPI_AEST_MEMORY_CONTROLLER_STRUCT;

///
/// SMMU structure.
///
typedef struct {
  /// AEST Node header
  EFI_ACPI_AEST_NODE_STRUCT    NodeHeader;

  /// Reference to the IORT table node that describes this SMMU.
  UINT32                       SmmuRefId;

  /// Reference to the IORT table node that is associated with the
  /// sub-component within this SMMU.
  UINT32                       SubComponentRefId;

  // Node Interface
  // EFI_ACPI_AEST_INTERFACE_STRUCT   NodeInterface;

  // Node Interrupt Array
  // EFI_ACPI_AEST_INTERRUPT_STRUCT   NodeInterruptArray[n];
} EFI_ACPI_AEST_SMMU_STRUCT;

///
/// Vendor-Defined structure.
///
typedef struct {
  /// AEST Node header
  EFI_ACPI_AEST_NODE_STRUCT    NodeHeader;

  /// ACPI HID of the component.
  UINT32                       HardwareId;

  /// The ACPI Unique identifier of the component.
  UINT32                       UniqueId;

  /// Vendor-specific data, for example to identify this error source.
  UINT8                        VendorData[16];

  // Node Interface
  // EFI_ACPI_AEST_INTERFACE_STRUCT   NodeInterface;

  // Node Interrupt Array
  // EFI_ACPI_AEST_INTERRUPT_STRUCT   NodeInterruptArray[n];
} EFI_ACPI_AEST_VENDOR_DEFINED_STRUCT;

///
/// GIC structure.
///
typedef struct {
  /// AEST Node header
  EFI_ACPI_AEST_NODE_STRUCT    NodeHeader;

  /// Type of GIC interface that is associated with this error node.
  ///   0x0 - GIC CPU (GICC)
  ///   0x1 - GIC Distributor (GICD)
  ///   0x2 - GIC Resistributor (GICR)
  ///   0x3 - GIC ITS (GITS)
  UINT32                       InterfaceType;

  /// Identifier for the interface instance.
  UINT32                       GicInterfaceRefId;

  // Node Interface
  // EFI_ACPI_AEST_INTERFACE_STRUCT   NodeInterface;

  // Node Interrupt Array
  // EFI_ACPI_AEST_INTERRUPT_STRUCT   NodeInterruptArray[n];
} EFI_ACPI_AEST_GIC_STRUCT;

// AEST GIC interface type definitions.
#define EFI_ACPI_AEST_GIC_INTERFACE_TYPE_GICC  0x0
#define EFI_ACPI_AEST_GIC_INTERFACE_TYPE_GICD  0x1
#define EFI_ACPI_AEST_GIC_INTERFACE_TYPE_GICR  0x2
#define EFI_ACPI_AEST_GIC_INTERFACE_TYPE_GITS  0x3

#pragma pack()

#endif // ARM_ERROR_SOURCE_TABLE_H_
