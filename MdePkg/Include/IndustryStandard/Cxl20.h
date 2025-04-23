/** @file
  CXL 2.0 Register definitions

  This file contains the register definitions based on the Compute Express Link
  (CXL) Specification Revision 2.0.

  Copyright (c) 2023, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CXL20_H_
#define CXL20_H_

#include <IndustryStandard/Cxl11.h>
#include <IndustryStandard/Acpi.h>

//
// CXL DVSEC IDs
// Compute Express Link Specification Revision 2.0 - Chapter 8.1.1
//
#define CXL_DVSEC_ID_PCIE_DVSEC_FOR_CXL_DEVICE         0x0
#define CXL_DVSEC_ID_NON_CXL_FUNCTION_MAP              0x2
#define CXL_DVSEC_ID_CXL20_EXTENSIONS_DVSEC_FOR_PORTS  0x3
#define CXL_DVSEC_ID_GPF_DVSEC_FOR_CXL_PORTS           0x4
#define CXL_DVSEC_ID_GPF_DVSEC_FOR_CXL_DEVICES         0x5
#define CXL_DVSEC_ID_PCIE_DVSEC_FOR_FLEX_BUS_PORT      0x7
#define CXL_DVSEC_ID_REGISTER_LOCATOR                  0x8
#define CXL_DVSEC_ID_MLD                               0x9
#define CXL_DVSEC_ID_PCIE_DVSEC_FOR_TEST_CAPABILITY    0xA

//
// Register Block ID
// Compute Express Link Specification Revision 2.0 - Chapter 8.1.9.1
//
#define CXL_REGISTER_BLOCK_ID_EMPTY                   0x0
#define CXL_REGISTER_BLOCK_ID_COMPONENT               0x1
#define CXL_REGISTER_BLOCK_ID_BAR_VIRTUALIZATION_ACL  0x2
#define CXL_REGISTER_BLOCK_ID_DEVICE                  0x3

//
// CXL component register layout
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.4
//
// |------------------------------------|
// |--------- Range & Type -------------|
// |------------------------------------| IO Base - 0KB
// |     (0KB - 4KB)IO Regs             |
// |------------------------------------| Cache and Mem Base - 4KB
// |     {4KB - 8KB)Cache & Mem Regs    |
// |------------------------------------| Implementation Spec Regs Base - 8KB
// |     (8KB - 56KB)Implement Spec Regs|
// |------------------------------------| ARB/Mux Regs Base - 56KB
// |     (56KB - 57KB)ARBMUX Regs       |
// |------------------------------------| Reserved Base - 57KB
// |     (57KB - 63KB)Reserved          |
// |------------------------------------| End 64KB
//
// Component Register Block Register Ranges Offset
//
#define CXL_COMPONENT_REGISTER_RANGE_OFFSET_IO         0x0
#define CXL_COMPONENT_REGISTER_RANGE_OFFSET_CACHE_MEM  0x1000
#define CXL_COMPONENT_REGISTER_RANGE_OFFSET_ARB_MUX    0xE000

//
// CXL Cache Memory Capability IDs
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.5
//
#define CXL_CACHE_MEM_CAPABILITY_ID_CXL                0x1
#define CXL_CACHE_MEM_CAPABILITY_ID_RAS                0x2
#define CXL_CACHE_MEM_CAPABILITY_ID_SECURITY           0x3
#define CXL_CACHE_MEM_CAPABILITY_ID_LINK               0x4
#define CXL_CACHE_MEM_CAPABILITY_ID_HDM_DECODER        0x5
#define CXL_CACHE_MEM_CAPABILITY_ID_EXTENDED_SECURITY  0x6
#define CXL_CACHE_MEM_CAPABILITY_ID_IDE                0x7
#define CXL_CACHE_MEM_CAPABILITY_ID_SNOOP_FILTER       0x8
#define CXL_CACHE_MEM_CAPABILITY_ID_MASK               0xFFFF

//
// Generic CXL Device Capability IDs 0x0000 ~ 0x3FFF
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.8.2.1
//
#define CXL_DEVICE_CAPABILITY_ID_CAPABILITIES_ARRAY_REGISTER  0x0000
#define CXL_DEVICE_CAPABILITY_ID_DEVICE_STATUS                0x0001
#define CXL_DEVICE_CAPABILITY_ID_PRIMARY_MAILBOX              0x0002
#define CXL_DEVICE_CAPABILITY_ID_SECONDARY_MAILBOX            0x0003

//
// Specific CXL Device Capability IDs 0x4000 ~ 0x7FFF
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.8.2.1 and 8.2.8.5

//
#define CXL_DEVICE_CAPABILITY_ID_MEMORY_DEVICE_STATUS  0x4000
#define CXL_DEVICE_CAPABILITY_ID_MASK                  0xFFFF

//
// Memory Device Status
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.8.5.1.1
//
#define CXL_MEM_DEVICE_MEDIA_STATUS_NOT_READY  0x0
#define CXL_MEM_DEVICE_MEDIA_STATUS_READY      0x1
#define CXL_MEM_DEVICE_MEDIA_STATUS_ERROR      0x2
#define CXL_MEM_DEVICE_MEDIA_STATUS_DISABLED   0x3

//
// "CEDT" CXL Early Discovery Table
// Compute Express Link Specification Revision 2.0  - Chapter 9.14.1
//
#define CXL_EARLY_DISCOVERY_TABLE_SIGNATURE  SIGNATURE_32 ('C', 'E', 'D', 'T')

#define CXL_EARLY_DISCOVERY_TABLE_REVISION_01  0x1

#define CEDT_TYPE_CHBS  0x0

//
// Ensure proper structure formats
//
#pragma pack(1)

//
// PCIe DVSEC for CXL Device
// Compute Express Link Specification Revision 2.0 - Chapter 8.1.3
//
typedef union {
  struct {
    UINT16    CacheCapable                       : 1;       // bit 0
    UINT16    IoCapable                          : 1;       // bit 1
    UINT16    MemCapable                         : 1;       // bit 2
    UINT16    MemHwInitMode                      : 1;       // bit 3
    UINT16    HdmCount                           : 2;       // bit 4..5
    UINT16    CacheWriteBackAndInvalidateCapable : 1;       // bit 6
    UINT16    CxlResetCapable                    : 1;       // bit 7
    UINT16    CxlResetTimeout                    : 3;       // bit 8..10
    UINT16    CxlResetMemClrCapable              : 1;       // bit 11
    UINT16    Reserved                           : 1;       // bit 12
    UINT16    MultipleLogicalDevice              : 1;       // bit 13
    UINT16    ViralCapable                       : 1;       // bit 14
    UINT16    PmInitCompletionReportingCapable   : 1;       // bit 15
  } Bits;
  UINT16    Uint16;
} CXL_DVSEC_CXL_DEVICE_CAPABILITY;

typedef union {
  struct {
    UINT16    CacheEnable        : 1;       // bit 0
    UINT16    IoEnable           : 1;       // bit 1
    UINT16    MemEnable          : 1;       // bit 2
    UINT16    CacheSfCoverage    : 5;       // bit 3..7
    UINT16    CacheSfGranularity : 3;       // bit 8..10
    UINT16    CacheCleanEviction : 1;       // bit 11
    UINT16    Reserved1          : 2;       // bit 12..13
    UINT16    ViralEnable        : 1;       // bit 14
    UINT16    Reserved2          : 1;       // bit 15
  } Bits;
  UINT16    Uint16;
} CXL_DVSEC_CXL_DEVICE_CONTROL;

typedef union {
  struct {
    UINT16    Reserved1   : 14;     // bit 0..13
    UINT16    ViralStatus : 1;      // bit 14
    UINT16    Reserved2   : 1;      // bit 15
  } Bits;
  UINT16    Uint16;
} CXL_DVSEC_CXL_DEVICE_STATUS;

typedef union {
  struct {
    UINT16    DisableCaching                      : 1;      // bit 0
    UINT16    InitiateCacheWriteBackAndInvalidate : 1;      // bit 1
    UINT16    InitiateCxlReset                    : 1;      // bit 2
    UINT16    CxlResetMemClrEnable                : 1;      // bit 3
    UINT16    Reserved                            : 12;     // bit 4..15
  } Bits;
  UINT16    Uint16;
} CXL_DVSEC_CXL_DEVICE_CONTROL2;

typedef union {
  struct {
    UINT16    CacheInvalid                         : 1;         // bit 0
    UINT16    CxlResetComplete                     : 1;         // bit 1
    UINT16    Reserved                             : 13;        // bit 2..14
    UINT16    PowerManagementInitialzationComplete : 1;         // bit 15
  } Bits;
  UINT16    Uint16;
} CXL_DVSEC_CXL_DEVICE_STATUS2;

typedef union {
  struct {
    UINT16    ConfigLock : 1;       // bit 0
    UINT16    Reserved   : 15;      // bit 1..15
  } Bits;
  UINT16    Uint16;
} CXL_DVSEC_CXL_DEVICE_LOCK;

typedef union {
  struct {
    UINT16    CacheSizeUnit : 4;        // bit 0..3
    UINT16    Reserved      : 4;        // bit 4..7
    UINT16    CacheSize     : 8;        // bit 8..15
  } Bits;
  UINT16    Uint16;
} CXL_DVSEC_CXL_DEVICE_CAPABILITY2;

typedef union {
  struct {
    UINT32    MemorySizeHigh : 32;      // bit 0..31
  } Bits;
  UINT32    Uint32;
} CXL_DVSEC_CXL_DEVICE_RANGE_SIZE_HIGH;

typedef union {
  struct {
    UINT32    MemoryInfoValid     : 1;      // bit 0
    UINT32    MemoryActive        : 1;      // bit 1
    UINT32    MediaType           : 3;      // bit 2..4
    UINT32    MemoryClass         : 3;      // bit 5..7
    UINT32    DesiredInterleave   : 5;      // bit 8..12
    UINT32    MemoryActiveTimeout : 3;      // bit 13..15
    UINT32    Reserved            : 12;     // bit 16..27
    UINT32    MemorySizeLow       : 4;      // bit 28..31
  } Bits;
  UINT32    Uint32;
} CXL_DVSEC_CXL_DEVICE_RANGE_SIZE_LOW;

typedef union {
  struct {
    UINT32    MemoryBaseHigh : 32;      // bit 0..31
  } Bits;
  UINT32    Uint32;
} CXL_DVSEC_CXL_DEVICE_RANGE_BASE_HIGH;

typedef union {
  struct {
    UINT32    Reserved      : 28;       // bit 0..27
    UINT32    MemoryBaseLow : 4;        // bit 28..31
  } Bits;
  UINT32    Uint32;
} CXL_DVSEC_CXL_DEVICE_RANGE_BASE_LOW;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER           Header;                                // offset 0x00
  PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_1    DvsecHeader1;                          // offset 0x04
  PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_2    DvsecHeader2;                          // offset 0x08
  CXL_DVSEC_CXL_DEVICE_CAPABILITY                    DeviceCapability;                      // offset 0x0A
  CXL_DVSEC_CXL_DEVICE_CONTROL                       DeviceControl;                         // offset 0x0C
  CXL_DVSEC_CXL_DEVICE_STATUS                        DeviceStatus;                          // offset 0x0E
  CXL_DVSEC_CXL_DEVICE_CONTROL2                      DeviceControl2;                        // offset 0x10
  CXL_DVSEC_CXL_DEVICE_STATUS2                       DeviceStatus2;                         // offset 0x12
  CXL_DVSEC_CXL_DEVICE_LOCK                          DeviceLock;                            // offset 0x14
  CXL_DVSEC_CXL_DEVICE_CAPABILITY2                   DeviceCapability2;                     // offset 0x16
  CXL_DVSEC_CXL_DEVICE_RANGE_SIZE_HIGH               DeviceRange1SizeHigh;                  // offset 0x18
  CXL_DVSEC_CXL_DEVICE_RANGE_SIZE_LOW                DeviceRange1SizeLow;                   // offset 0x1C
  CXL_DVSEC_CXL_DEVICE_RANGE_BASE_HIGH               DeviceRange1BaseHigh;                  // offset 0x20
  CXL_DVSEC_CXL_DEVICE_RANGE_BASE_LOW                DeviceRange1BaseLow;                   // offset 0x24
  CXL_DVSEC_CXL_DEVICE_RANGE_SIZE_HIGH               DeviceRange2SizeHigh;                  // offset 0x28
  CXL_DVSEC_CXL_DEVICE_RANGE_SIZE_LOW                DeviceRange2SizeLow;                   // offset 0x2C
  CXL_DVSEC_CXL_DEVICE_RANGE_BASE_HIGH               DeviceRange2BaseHigh;                  // offset 0x30
  CXL_DVSEC_CXL_DEVICE_RANGE_BASE_LOW                DeviceRange2BaseLow;                   // offset 0x34
} CXL_DVSEC_CXL_DEVICE;

#define CXL_DVSEC_CXL_DEVICE_REVISION_1  0x1

//
// Register Locator DVSEC
// Compute Express Link Specification Revision 2.0 - Chapter 8.1.9
//

typedef union {
  struct {
    UINT32    RegisterBir             : 3;      // bit 0..2
    UINT32    Reserved                : 5;      // bit 3..7
    UINT32    RegisterBlockIdentifier : 8;      // bit 8..15
    UINT32    RegisterBlockOffsetLow  : 16;     // bit 16..31
  } Bits;
  UINT32    Uint32;
} CXL_DVSEC_REGISTER_LOCATOR_REGISTER_OFFSET_LOW;

typedef union {
  struct {
    UINT32    RegisterBlockOffsetHigh : 32;     // bit 0..31
  } Bits;
  UINT32    Uint32;
} CXL_DVSEC_REGISTER_LOCATOR_REGISTER_OFFSET_HIGH;

typedef struct {
  CXL_DVSEC_REGISTER_LOCATOR_REGISTER_OFFSET_LOW     OffsetLow;
  CXL_DVSEC_REGISTER_LOCATOR_REGISTER_OFFSET_HIGH    OffsetHigh;
} CXL_DVSEC_REGISTER_LOCATOR_REGISTER_BLOCK;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER           Header;                                   // offset 0x00
  PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_1    DvsecHeader1;                             // offset 0x04
  PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_2    DvsecHeader2;                             // offset 0x08
  UINT16                                             Reserved;                                 // offset 0x0A
  CXL_DVSEC_REGISTER_LOCATOR_REGISTER_BLOCK          RegisterBlock[];                          // offset 0x0C
} CXL_DVSEC_REGISTER_LOCATOR;

#define CXL_DVSEC_REGISTER_LOCATOR_REVISION_0  0x0

//
// CXL HDM Decoder Capability Header Register
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.5.5
//
typedef union {
  struct {
    UINT32    CxlCapabilityId                : 16;      // bit 0..15
    UINT32    CxlCapabilityVersion           :  4;      // bit 16..19
    UINT32    CxlHdmDecoderCapabilityPointer : 12;      // bit 20..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_CAPABILITY_HEADER_REGISTER;

//
// CXL HDM Decoder Capability Register
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.5.12
//
typedef union {
  struct {
    UINT32    DecoderCount                  : 4;        // bit 0..3
    UINT32    TargetCount                   : 4;        // bit 4..7
    UINT32    InterleaveCapableA11to8       : 1;        // bit 8
    UINT32    InterleaveCapableA14to12      : 1;        // bit 9
    UINT32    PoisonOnDecodeErrorCapability : 1;        // bit 10
    UINT32    Reserved                      : 21;       // bit 11..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_CAPABILITY_REGISTER;

typedef union {
  struct {
    UINT32    PoisonOnDecodeErrorEnable : 1;        // bit 0
    UINT32    HdmDecoderEnable          : 1;        // bit 1
    UINT32    Reserved                  : 30;       // bit 2..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_GLOBAL_CONTROL_REGISTER;

typedef union {
  struct {
    UINT32    Reserved      : 28;       // bit 0..27
    UINT32    MemoryBaseLow : 4;        // bit 28..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_BASE_LOW_REGISTER;

typedef union {
  struct {
    UINT32    MemoryBaseHigh : 32;      // bit 0..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_BASE_HIGH_REGISTER;

typedef union {
  struct {
    UINT32    Reserved      : 28;       // bit 0..27
    UINT32    MemorySizeLow : 4;        // bit 28..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_SIZE_LOW_REGISTER;

typedef union {
  struct {
    UINT32    MemorySizeHigh : 32;      // bit 0..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_SIZE_HIGH_REGISTER;

typedef union {
  struct {
    UINT32    InterleaveGranularity : 4;        // bit 0..3
    UINT32    InterleaveWays        : 4;        // bit 4..7
    UINT32    LockOnCommit          : 1;        // bit 8
    UINT32    Commit                : 1;        // bit 9
    UINT32    Committed             : 1;        // bit 10
    UINT32    ErrorNotCommitted     : 1;        // bit 11
    UINT32    TargetDeviceType      : 1;        // bit 12
    UINT32    Reserved              : 19;       // bit 13..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_CONTROL_REGISTER;

typedef union {
  struct {
    UINT32    TargetPortIdentiferWay0 : 8;      // bit 0..7
    UINT32    TargetPortIdentiferWay1 : 8;      // bit 8..15
    UINT32    TargetPortIdentiferWay2 : 8;      // bit 16..23
    UINT32    TargetPortIdentiferWay3 : 8;      // bit 24..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_TARGET_LIST_LOW_REGISTER;

typedef union {
  struct {
    UINT32    Reserved   : 28;          // bit 0..27
    UINT32    DpaSkipLow : 4;           // bit 28..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_DPA_SKIP_LOW_REGISTER;

typedef union {
  struct {
    UINT32    TargetPortIdentiferWay4 : 8;      // bit 0..7
    UINT32    TargetPortIdentiferWay5 : 8;      // bit 8..15
    UINT32    TargetPortIdentiferWay6 : 8;      // bit 16..23
    UINT32    TargetPortIdentiferWay7 : 8;      // bit 24..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_TARGET_LIST_HIGH_REGISTER;

typedef union {
  struct {
    UINT32    DpaSkipHigh : 32;     // bit 0..31
  } Bits;
  UINT32    Uint32;
} CXL_HDM_DECODER_DPA_SKIP_HIGH_REGISTER;

typedef union {
  CXL_HDM_DECODER_TARGET_LIST_LOW_REGISTER    TargetListLow;
  CXL_HDM_DECODER_DPA_SKIP_LOW_REGISTER       DpaSkipLow;
} CXL_HDM_DECODER_TARGET_LIST_OR_DPA_SKIP_LOW;

typedef union {
  CXL_HDM_DECODER_TARGET_LIST_HIGH_REGISTER    TargetListHigh;
  CXL_HDM_DECODER_DPA_SKIP_HIGH_REGISTER       DpaSkipHigh;
} CXL_HDM_DECODER_TARGET_LIST_OR_DPA_SKIP_HIGH;

typedef struct {
  CXL_HDM_DECODER_BASE_LOW_REGISTER               DecoderBaseLow;                 // 0x10
  CXL_HDM_DECODER_BASE_HIGH_REGISTER              DecoderBaseHigh;                // 0x14
  CXL_HDM_DECODER_SIZE_LOW_REGISTER               DecoderSizeLow;                 // 0x18
  CXL_HDM_DECODER_SIZE_HIGH_REGISTER              DecoderSizeHigh;                // 0x1c
  CXL_HDM_DECODER_CONTROL_REGISTER                DecoderControl;                 // 0x20
  CXL_HDM_DECODER_TARGET_LIST_OR_DPA_SKIP_LOW     DecoderTargetListDpaSkipLow;    // 0x24
  CXL_HDM_DECODER_TARGET_LIST_OR_DPA_SKIP_HIGH    DecoderTargetListDpaSkipHigh;   // 0x28
  UINT32                                          Reserved;                       // 0x2C
} CXL_HDM_DECODER;

//
// CXL Device Capabilities Array Register
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.8.1
//

typedef union {
  struct {
    UINT64    CxlDeviceCapabilityId      : 16;      // bit 0..15
    UINT64    CxlDeviceCapabilityVersion : 8;       // bit 16..23
    UINT64    Reserved1                  : 8;       // bit 24..31
    UINT64    CxlDeviceCapabilitiesCount : 16;      // bit 32..47
    UINT64    Reserved2                  : 16;      // bit 48..63
  } Bits;
  UINT64    Uint64;
} CXL_DEVICE_CAPABILITIES_ARRAY_REGISTER;

//
// CXL Memory Status Register
// Compute Express Link Specification Revision 2.0 - Chapter 8.2.8.5
//
typedef union {
  struct {
    UINT64    DeviceFatal            : 1;       // bit 0
    UINT64    FwHalt                 : 1;       // bit 1
    UINT64    MediaStatus            : 2;       // bit 2..3
    UINT64    MailboxInterfacesReady : 1;       // bit 4
    UINT64    ResetNeeded            : 3;       // bit 5..7
    UINT64    Reserved               : 56;      // bit 8..63
  } Bits;
  UINT64    Uint64;
} CXL_MEMORY_DEVICE_STATUS_REGISTER;

//
// CEDT header
// Compute Express Link Specification Revision 2.0  - Chapter 9.14.1.1
//
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    Header;
} CXL_EARLY_DISCOVERY_TABLE;

//
// Node header definition shared by all CEDT structure types
//
typedef struct {
  UINT8     Type;
  UINT8     Reserved;
  UINT16    Length;
} CEDT_STRUCTURE;

//
// Definition for CXL Host Bridge Structure (CHBS)
// Compute Express Link Specification Revision 2.0  - Chapter 9.14.1.2
//
typedef struct {
  CEDT_STRUCTURE    Header;
  UINT32            Uid;
  UINT32            CxlVersion;
  UINT32            Reserved;
  UINT64            Base;
  UINT64            Length;
} CXL_HOST_BRIDGE_STRUCTURE;

#pragma pack()

#endif
