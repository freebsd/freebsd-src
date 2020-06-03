/** @file
  GUIDs and definitions used for Common Platform Error Record.

  Copyright (c) 2011 - 2017, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2016 Hewlett Packard Enterprise Development LP<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUIDs defined in UEFI 2.7 Specification.

**/

#ifndef __CPER_GUID_H__
#define __CPER_GUID_H__

#pragma pack(1)

#define EFI_ERROR_RECORD_SIGNATURE_START   SIGNATURE_32('C', 'P', 'E', 'R')
#define EFI_ERROR_RECORD_SIGNATURE_END     0xFFFFFFFF

#define EFI_ERROR_RECORD_REVISION          0x0101

///
/// Error Severity in Error Record Header and Error Section Descriptor
///@{
#define EFI_GENERIC_ERROR_RECOVERABLE                0x00000000
#define EFI_GENERIC_ERROR_FATAL                      0x00000001
#define EFI_GENERIC_ERROR_CORRECTED                  0x00000002
#define EFI_GENERIC_ERROR_INFO                       0x00000003
///@}

///
/// The validation bit mask indicates the validity of the following fields
/// in Error Record Header.
///@{
#define EFI_ERROR_RECORD_HEADER_PLATFORM_ID_VALID    BIT0
#define EFI_ERROR_RECORD_HEADER_TIME_STAMP_VALID     BIT1
#define EFI_ERROR_RECORD_HEADER_PARTITION_ID_VALID   BIT2
///@}

///
/// Timestamp is precise if this bit is set and correlates to the time of the
/// error event.
///
#define EFI_ERROR_TIME_STAMP_PRECISE                 BIT0

///
/// The timestamp correlates to the time when the error information was collected
/// by the system software and may not necessarily represent the time of the error
/// event. The timestamp contains the local time in BCD format.
///
typedef struct {
  UINT8              Seconds;
  UINT8              Minutes;
  UINT8              Hours;
  UINT8              Flag;
  UINT8              Day;
  UINT8              Month;
  UINT8              Year;
  UINT8              Century;
} EFI_ERROR_TIME_STAMP;

///
/// GUID value indicating the record association with an error event notification type.
///@{
#define EFI_EVENT_NOTIFICATION_TYEP_CMC_GUID \
  { \
    0x2DCE8BB1, 0xBDD7, 0x450e, { 0xB9, 0xAD, 0x9C, 0xF4, 0xEB, 0xD4, 0xF8, 0x90 } \
  }
#define EFI_EVENT_NOTIFICATION_TYEP_CPE_GUID \
  { \
    0x4E292F96, 0xD843, 0x4a55, { 0xA8, 0xC2, 0xD4, 0x81, 0xF2, 0x7E, 0xBE, 0xEE } \
  }
#define EFI_EVENT_NOTIFICATION_TYEP_MCE_GUID \
  { \
    0xE8F56FFE, 0x919C, 0x4cc5, { 0xBA, 0x88, 0x65, 0xAB, 0xE1, 0x49, 0x13, 0xBB } \
  }
#define EFI_EVENT_NOTIFICATION_TYEP_PCIE_GUID \
  { \
    0xCF93C01F, 0x1A16, 0x4dfc, { 0xB8, 0xBC, 0x9C, 0x4D, 0xAF, 0x67, 0xC1, 0x04 } \
  }
#define EFI_EVENT_NOTIFICATION_TYEP_INIT_GUID \
  { \
    0xCC5263E8, 0x9308, 0x454a, { 0x89, 0xD0, 0x34, 0x0B, 0xD3, 0x9B, 0xC9, 0x8E } \
  }
#define EFI_EVENT_NOTIFICATION_TYEP_NMI_GUID \
  { \
    0x5BAD89FF, 0xB7E6, 0x42c9, { 0x81, 0x4A, 0xCF, 0x24, 0x85, 0xD6, 0xE9, 0x8A } \
  }
#define EFI_EVENT_NOTIFICATION_TYEP_BOOT_GUID \
  { \
    0x3D61A466, 0xAB40, 0x409a, { 0xA6, 0x98, 0xF3, 0x62, 0xD4, 0x64, 0xB3, 0x8F } \
  }
#define EFI_EVENT_NOTIFICATION_TYEP_DMAR_GUID \
  { \
    0x667DD791, 0xC6B3, 0x4c27, { 0x8A, 0x6B, 0x0F, 0x8E, 0x72, 0x2D, 0xEB, 0x41 } \
  }
#define EFI_EVENT_NOTIFICATION_TYPE_DMAR_SEA \
  { \
    0x9A78788A, 0xBBE8, 0x11E4, { 0x80, 0x9E, 0x67, 0x61, 0x1E, 0x5D, 0x46, 0xB0 } \
  }
#define EFI_EVENT_NOTIFICATION_TYPE_DMAR_SEI \
  { \
    0x5C284C81, 0xB0AE, 0x4E87, { 0xA3, 0x22, 0xB0, 0x4C, 0x85, 0x62, 0x43, 0x23 } \
  }
#define EFI_EVENT_NOTIFICATION_TYPE_DMAR_PEI \
  { \
    0x09A9D5AC, 0x5204, 0x4214, { 0x96, 0xE5, 0x94, 0x99, 0x2E, 0x75, 0x2B, 0xCD } \
  }
///@}

///
/// Error Record Header Flags
///@{
#define EFI_HW_ERROR_FLAGS_RECOVERED                 0x00000001
#define EFI_HW_ERROR_FLAGS_PREVERR                   0x00000002
#define EFI_HW_ERROR_FLAGS_SIMULATED                 0x00000004
///@}

///
/// Common error record header
///
typedef struct {
  UINT32               SignatureStart;
  UINT16               Revision;
  UINT32               SignatureEnd;
  UINT16               SectionCount;
  UINT32               ErrorSeverity;
  UINT32               ValidationBits;
  UINT32               RecordLength;
  EFI_ERROR_TIME_STAMP TimeStamp;
  EFI_GUID             PlatformID;
  EFI_GUID             PartitionID;
  EFI_GUID             CreatorID;
  EFI_GUID             NotificationType;
  UINT64               RecordID;
  UINT32               Flags;
  UINT64               PersistenceInfo;
  UINT8                Resv1[12];
  ///
  /// An array of SectionCount descriptors for the associated
  /// sections. The number of valid sections is equivalent to the
  /// SectionCount. The buffer size of the record may include
  /// more space to dynamically add additional Section
  /// Descriptors to the error record.
  ///
} EFI_COMMON_ERROR_RECORD_HEADER;

#define EFI_ERROR_SECTION_REVISION  0x0100

///
/// Validity Fields in Error Section Descriptor.
///
#define EFI_ERROR_SECTION_FRU_ID_VALID               BIT0
#define EFI_ERROR_SECTION_FRU_STRING_VALID           BIT1

///
/// Flag field contains information that describes the error section
/// in Error Section Descriptor.
///
#define EFI_ERROR_SECTION_FLAGS_PRIMARY                        BIT0
#define EFI_ERROR_SECTION_FLAGS_CONTAINMENT_WARNING            BIT1
#define EFI_ERROR_SECTION_FLAGS_RESET                          BIT2
#define EFI_ERROR_SECTION_FLAGS_ERROR_THRESHOLD_EXCEEDED       BIT3
#define EFI_ERROR_SECTION_FLAGS_RESOURCE_NOT_ACCESSIBLE        BIT4
#define EFI_ERROR_SECTION_FLAGS_LATENT_ERROR                   BIT5

///
/// Error Sectition Type GUIDs in Error Section Descriptor
///@{
#define EFI_ERROR_SECTION_PROCESSOR_GENERIC_GUID \
  { \
    0x9876ccad, 0x47b4, 0x4bdb, { 0xb6, 0x5e, 0x16, 0xf1, 0x93, 0xc4, 0xf3, 0xdb } \
  }
#define EFI_ERROR_SECTION_PROCESSOR_SPECIFIC_GUID \
  { \
    0xdc3ea0b0, 0xa144, 0x4797, { 0xb9, 0x5b, 0x53, 0xfa, 0x24, 0x2b, 0x6e, 0x1d } \
  }
#define EFI_ERROR_SECTION_PROCESSOR_SPECIFIC_IA32X64_GUID \
  { \
    0xdc3ea0b0, 0xa144, 0x4797, { 0xb9, 0x5b, 0x53, 0xfa, 0x24, 0x2b, 0x6e, 0x1d } \
  }
#define EFI_ERROR_SECTION_PROCESSOR_SPECIFIC_ARM_GUID \
  { \
    0xe19e3d16, 0xbc11, 0x11e4, { 0x9c, 0xaa, 0xc2, 0x05, 0x1d, 0x5d, 0x46, 0xb0 } \
  }
#define EFI_ERROR_SECTION_PLATFORM_MEMORY_GUID \
  { \
    0xa5bc1114, 0x6f64, 0x4ede, { 0xb8, 0x63, 0x3e, 0x83, 0xed, 0x7c, 0x83, 0xb1 } \
  }
#define EFI_ERROR_SECTION_PLATFORM_MEMORY2_GUID \
  { \
    0x61EC04FC, 0x48E6, 0xD813, { 0x25, 0xC9, 0x8D, 0xAA, 0x44, 0x75, 0x0B, 0x12 } \
  }
#define EFI_ERROR_SECTION_PCIE_GUID \
  { \
    0xd995e954, 0xbbc1, 0x430f, { 0xad, 0x91, 0xb4, 0x4d, 0xcb, 0x3c, 0x6f, 0x35 } \
  }
#define EFI_ERROR_SECTION_FW_ERROR_RECORD_GUID \
  { \
    0x81212a96, 0x09ed, 0x4996, { 0x94, 0x71, 0x8d, 0x72, 0x9c, 0x8e, 0x69, 0xed } \
  }
#define EFI_ERROR_SECTION_PCI_PCIX_BUS_GUID \
  { \
    0xc5753963, 0x3b84, 0x4095, { 0xbf, 0x78, 0xed, 0xda, 0xd3, 0xf9, 0xc9, 0xdd } \
  }
#define EFI_ERROR_SECTION_PCI_DEVICE_GUID \
  { \
    0xeb5e4685, 0xca66, 0x4769, { 0xb6, 0xa2, 0x26, 0x06, 0x8b, 0x00, 0x13, 0x26 } \
  }
#define EFI_ERROR_SECTION_DMAR_GENERIC_GUID \
  { \
    0x5b51fef7, 0xc79d, 0x4434, { 0x8f, 0x1b, 0xaa, 0x62, 0xde, 0x3e, 0x2c, 0x64 } \
  }
#define EFI_ERROR_SECTION_DIRECTED_IO_DMAR_GUID \
  { \
    0x71761d37, 0x32b2, 0x45cd, { 0xa7, 0xd0, 0xb0, 0xfe, 0xdd, 0x93, 0xe8, 0xcf } \
  }
#define EFI_ERROR_SECTION_IOMMU_DMAR_GUID \
  { \
    0x036f84e1, 0x7f37, 0x428c, { 0xa7, 0x9e, 0x57, 0x5f, 0xdf, 0xaa, 0x84, 0xec } \
  }
///@}

///
/// Error Section Descriptor
///
typedef struct {
  UINT32                 SectionOffset;
  UINT32                 SectionLength;
  UINT16                 Revision;
  UINT8                  SecValidMask;
  UINT8                  Resv1;
  UINT32                 SectionFlags;
  EFI_GUID               SectionType;
  EFI_GUID               FruId;
  UINT32                 Severity;
  CHAR8                  FruString[20];
} EFI_ERROR_SECTION_DESCRIPTOR;

///
/// The validation bit mask indicates whether or not each of the following fields are
/// valid in Proessor Generic Error section.
///@{
#define EFI_GENERIC_ERROR_PROC_TYPE_VALID            BIT0
#define EFI_GENERIC_ERROR_PROC_ISA_VALID             BIT1
#define EFI_GENERIC_ERROR_PROC_ERROR_TYPE_VALID      BIT2
#define EFI_GENERIC_ERROR_PROC_OPERATION_VALID       BIT3
#define EFI_GENERIC_ERROR_PROC_FLAGS_VALID           BIT4
#define EFI_GENERIC_ERROR_PROC_LEVEL_VALID           BIT5
#define EFI_GENERIC_ERROR_PROC_VERSION_VALID         BIT6
#define EFI_GENERIC_ERROR_PROC_BRAND_VALID           BIT7
#define EFI_GENERIC_ERROR_PROC_ID_VALID              BIT8
#define EFI_GENERIC_ERROR_PROC_TARGET_ADDR_VALID     BIT9
#define EFI_GENERIC_ERROR_PROC_REQUESTER_ID_VALID    BIT10
#define EFI_GENERIC_ERROR_PROC_RESPONDER_ID_VALID    BIT11
#define EFI_GENERIC_ERROR_PROC_INST_IP_VALID         BIT12
///@}

///
/// The type of the processor architecture in Proessor Generic Error section.
///@{
#define EFI_GENERIC_ERROR_PROC_TYPE_IA32_X64         0x00
#define EFI_GENERIC_ERROR_PROC_TYPE_IA64             0x01
#define EFI_GENERIC_ERROR_PROC_TYPE_ARM              0x02
///@}

///
/// The type of the instruction set executing when the error occurred in Proessor
/// Generic Error section.
///@{
#define EFI_GENERIC_ERROR_PROC_ISA_IA32              0x00
#define EFI_GENERIC_ERROR_PROC_ISA_IA64              0x01
#define EFI_GENERIC_ERROR_PROC_ISA_X64               0x02
#define EFI_GENERIC_ERROR_PROC_ISA_ARM_A32_T32       0x03
#define EFI_GENERIC_ERROR_PROC_ISA_ARM_A64           0x04
///@}

///
/// The type of error that occurred in Proessor Generic Error section.
///@{
#define EFI_GENERIC_ERROR_PROC_ERROR_TYPE_UNKNOWN    0x00
#define EFI_GENERIC_ERROR_PROC_ERROR_TYPE_CACHE      0x01
#define EFI_GENERIC_ERROR_PROC_ERROR_TYPE_TLB        0x02
#define EFI_GENERIC_ERROR_PROC_ERROR_TYPE_BUS        0x04
#define EFI_GENERIC_ERROR_PROC_ERROR_TYPE_MICRO_ARCH 0x08
///@}

///
/// The type of operation in Proessor Generic Error section.
///@{
#define EFI_GENERIC_ERROR_PROC_OPERATION_GENERIC               0x00
#define EFI_GENERIC_ERROR_PROC_OPERATION_DATA_READ             0x01
#define EFI_GENERIC_ERROR_PROC_OPERATION_DATA_WRITE            0x02
#define EFI_GENERIC_ERROR_PROC_OPERATION_INSTRUCTION_EXEC      0x03
///@}

///
/// Flags bit mask indicates additional information about the error in Proessor Generic
/// Error section
///@{
#define EFI_GENERIC_ERROR_PROC_FLAGS_RESTARTABLE     BIT0
#define EFI_GENERIC_ERROR_PROC_FLAGS_PRECISE_IP      BIT1
#define EFI_GENERIC_ERROR_PROC_FLAGS_OVERFLOW        BIT2
#define EFI_GENERIC_ERROR_PROC_FLAGS_CORRECTED       BIT3
///@}

///
/// Processor Generic Error Section
/// describes processor reported hardware errors for logical processors in the system.
///
typedef struct {
  UINT64             ValidFields;
  UINT8              Type;
  UINT8              Isa;
  UINT8              ErrorType;
  UINT8              Operation;
  UINT8              Flags;
  UINT8              Level;
  UINT16             Resv1;
  UINT64             VersionInfo;
  CHAR8              BrandString[128];
  UINT64             ApicId;
  UINT64             TargetAddr;
  UINT64             RequestorId;
  UINT64             ResponderId;
  UINT64             InstructionIP;
} EFI_PROCESSOR_GENERIC_ERROR_DATA;


#if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64)
///
/// IA32 and x64 Specific definitions.
///

///
/// GUID value indicating the type of Processor Error Information structure
/// in IA32/X64 Processor Error Information Structure.
///@{
#define EFI_IA32_X64_ERROR_TYPE_CACHE_CHECK_GUID \
  { \
    0xA55701F5, 0xE3EF, 0x43de, {0xAC, 0x72, 0x24, 0x9B, 0x57, 0x3F, 0xAD, 0x2C } \
  }
#define EFI_IA32_X64_ERROR_TYPE_TLB_CHECK_GUID \
  { \
    0xFC06B535, 0x5E1F, 0x4562, {0x9F, 0x25, 0x0A, 0x3B, 0x9A, 0xDB, 0x63, 0xC3 } \
  }
#define EFI_IA32_X64_ERROR_TYPE_BUS_CHECK_GUID \
  { \
    0x1CF3F8B3, 0xC5B1, 0x49a2, {0xAA, 0x59, 0x5E, 0xEF, 0x92, 0xFF, 0xA6, 0x3C } \
  }
#define EFI_IA32_X64_ERROR_TYPE_MS_CHECK_GUID \
  { \
    0x48AB7F57, 0xDC34, 0x4f6c, {0xA7, 0xD3, 0xB0, 0xB5, 0xB0, 0xA7, 0x43, 0x14 } \
  }
///@}

///
/// The validation bit mask indicates which fields in the IA32/X64 Processor
/// Error Record structure are valid.
///@{
#define EFI_IA32_X64_PROCESSOR_ERROR_APIC_ID_VALID         BIT0
#define EFI_IA32_X64_PROCESSOR_ERROR_CPU_ID_INFO_VALID     BIT1
///@}

///
/// IA32/X64 Processor Error Record
///
typedef struct {
  UINT64             ValidFields;
  UINT64             ApicId;
  UINT8              CpuIdInfo[48];
} EFI_IA32_X64_PROCESSOR_ERROR_RECORD;

///
/// The validation bit mask indicates which fields in the Cache Check structure
/// are valid.
///@{
#define EFI_CACHE_CHECK_TRANSACTION_TYPE_VALID       BIT0
#define EFI_CACHE_CHECK_OPERATION_VALID              BIT1
#define EFI_CACHE_CHECK_LEVEL_VALID                  BIT2
#define EFI_CACHE_CHECK_CONTEXT_CORRUPT_VALID        BIT3
#define EFI_CACHE_CHECK_UNCORRECTED_VALID            BIT4
#define EFI_CACHE_CHECK_PRECISE_IP_VALID             BIT5
#define EFI_CACHE_CHECK_RESTARTABLE_VALID            BIT6
#define EFI_CACHE_CHECK_OVERFLOW_VALID               BIT7
///@}

///
/// Type of cache error in the Cache Check structure
///@{
#define EFI_CACHE_CHECK_ERROR_TYPE_INSTRUCTION       0
#define EFI_CACHE_CHECK_ERROR_TYPE_DATA_ACCESS       1
#define EFI_CACHE_CHECK_ERROR_TYPE_GENERIC           2
///@}

///
/// Type of cache operation that caused the error in the Cache
/// Check structure
///@{
#define EFI_CACHE_CHECK_OPERATION_TYPE_GENERIC                 0
#define EFI_CACHE_CHECK_OPERATION_TYPE_GENERIC_READ            1
#define EFI_CACHE_CHECK_OPERATION_TYPE_GENERIC_WRITE           2
#define EFI_CACHE_CHECK_OPERATION_TYPE_DATA_READ               3
#define EFI_CACHE_CHECK_OPERATION_TYPE_DATA_WRITE              4
#define EFI_CACHE_CHECK_OPERATION_TYPE_INSTRUCTION_FETCH       5
#define EFI_CACHE_CHECK_OPERATION_TYPE_PREFETCH                6
#define EFI_CACHE_CHECK_OPERATION_TYPE_EVICTION                7
#define EFI_CACHE_CHECK_OPERATION_TYPE_SNOOP                   8
///@}

///
/// IA32/X64 Cache Check Structure
///
typedef struct {
  UINT64             ValidFields:16;
  UINT64             TransactionType:2;
  UINT64             Operation:4;
  UINT64             Level:3;
  UINT64             ContextCorrupt:1;
  UINT64             ErrorUncorrected:1;
  UINT64             PreciseIp:1;
  UINT64             RestartableIp:1;
  UINT64             Overflow:1;
  UINT64             Resv1:34;
} EFI_IA32_X64_CACHE_CHECK_INFO;

///
/// The validation bit mask indicates which fields in the TLB Check structure
/// are valid.
///@{
#define EFI_TLB_CHECK_TRANSACTION_TYPE_VALID         BIT0
#define EFI_TLB_CHECK_OPERATION_VALID                BIT1
#define EFI_TLB_CHECK_LEVEL_VALID                    BIT2
#define EFI_TLB_CHECK_CONTEXT_CORRUPT_VALID          BIT3
#define EFI_TLB_CHECK_UNCORRECTED_VALID              BIT4
#define EFI_TLB_CHECK_PRECISE_IP_VALID               BIT5
#define EFI_TLB_CHECK_RESTARTABLE_VALID              BIT6
#define EFI_TLB_CHECK_OVERFLOW_VALID                 BIT7
///@}

///
/// Type of cache error in the TLB Check structure
///@{
#define EFI_TLB_CHECK_ERROR_TYPE_INSTRUCTION         0
#define EFI_TLB_CHECK_ERROR_TYPE_DATA_ACCESS         1
#define EFI_TLB_CHECK_ERROR_TYPE_GENERIC             2
///@}

///
/// Type of cache operation that caused the error in the TLB
/// Check structure
///@{
#define EFI_TLB_CHECK_OPERATION_TYPE_GENERIC         0
#define EFI_TLB_CHECK_OPERATION_TYPE_GENERIC_READ    1
#define EFI_TLB_CHECK_OPERATION_TYPE_GENERIC_WRITE   2
#define EFI_TLB_CHECK_OPERATION_TYPE_DATA_READ       3
#define EFI_TLB_CHECK_OPERATION_TYPE_DATA_WRITE      4
#define EFI_TLB_CHECK_OPERATION_TYPE_INST_FETCH      5
#define EFI_TLB_CHECK_OPERATION_TYPE_PREFETCH        6
///@}

///
/// IA32/X64 TLB Check Structure
///
typedef struct {
  UINT64             ValidFields:16;
  UINT64             TransactionType:2;
  UINT64             Operation:4;
  UINT64             Level:3;
  UINT64             ContextCorrupt:1;
  UINT64             ErrorUncorrected:1;
  UINT64             PreciseIp:1;
  UINT64             RestartableIp:1;
  UINT64             Overflow:1;
  UINT64             Resv1:34;
} EFI_IA32_X64_TLB_CHECK_INFO;

///
/// The validation bit mask indicates which fields in the MS Check structure
/// are valid.
///@{
#define EFI_BUS_CHECK_TRANSACTION_TYPE_VALID         BIT0
#define EFI_BUS_CHECK_OPERATION_VALID                BIT1
#define EFI_BUS_CHECK_LEVEL_VALID                    BIT2
#define EFI_BUS_CHECK_CONTEXT_CORRUPT_VALID          BIT3
#define EFI_BUS_CHECK_UNCORRECTED_VALID              BIT4
#define EFI_BUS_CHECK_PRECISE_IP_VALID               BIT5
#define EFI_BUS_CHECK_RESTARTABLE_VALID              BIT6
#define EFI_BUS_CHECK_OVERFLOW_VALID                 BIT7
#define EFI_BUS_CHECK_PARTICIPATION_TYPE_VALID       BIT8
#define EFI_BUS_CHECK_TIME_OUT_VALID                 BIT9
#define EFI_BUS_CHECK_ADDRESS_SPACE_VALID            BIT10
///@}

///
/// Type of cache error in the Bus Check structure
///@{
#define EFI_BUS_CHECK_ERROR_TYPE_INSTRUCTION         0
#define EFI_BUS_CHECK_ERROR_TYPE_DATA_ACCESS         1
#define EFI_BUS_CHECK_ERROR_TYPE_GENERIC             2
///@}

///
/// Type of cache operation that caused the error in the Bus
/// Check structure
///@{
#define EFI_BUS_CHECK_OPERATION_TYPE_GENERIC         0
#define EFI_BUS_CHECK_OPERATION_TYPE_GENERIC_READ    1
#define EFI_BUS_CHECK_OPERATION_TYPE_GENERIC_WRITE   2
#define EFI_BUS_CHECK_OPERATION_TYPE_DATA_READ       3
#define EFI_BUS_CHECK_OPERATION_TYPE_DATA_WRITE      4
#define EFI_BUS_CHECK_OPERATION_TYPE_INST_FETCH      5
#define EFI_BUS_CHECK_OPERATION_TYPE_PREFETCH        6
///@}

///
/// Type of Participation
///@{
#define EFI_BUS_CHECK_PARTICIPATION_TYPE_REQUEST     0
#define EFI_BUS_CHECK_PARTICIPATION_TYPE_RESPONDED   1
#define EFI_BUS_CHECK_PARTICIPATION_TYPE_OBSERVED    2
#define EFI_BUS_CHECK_PARTICIPATION_TYPE_GENERIC     3
///@}

///
/// Type of Address Space
///@{
#define EFI_BUS_CHECK_ADDRESS_SPACE_TYPE_MEMORY      0
#define EFI_BUS_CHECK_ADDRESS_SPACE_TYPE_RESERVED    1
#define EFI_BUS_CHECK_ADDRESS_SPACE_TYPE_IO          2
#define EFI_BUS_CHECK_ADDRESS_SPACE_TYPE_OTHER       3
///@}

///
/// IA32/X64 Bus Check Structure
///
typedef struct {
  UINT64             ValidFields:16;
  UINT64             TransactionType:2;
  UINT64             Operation:4;
  UINT64             Level:3;
  UINT64             ContextCorrupt:1;
  UINT64             ErrorUncorrected:1;
  UINT64             PreciseIp:1;
  UINT64             RestartableIp:1;
  UINT64             Overflow:1;
  UINT64             ParticipationType:2;
  UINT64             TimeOut:1;
  UINT64             AddressSpace:2;
  UINT64             Resv1:29;
} EFI_IA32_X64_BUS_CHECK_INFO;

///
/// The validation bit mask indicates which fields in the MS Check structure
/// are valid.
///@{
#define EFI_MS_CHECK_ERROR_TYPE_VALID                BIT0
#define EFI_MS_CHECK_CONTEXT_CORRUPT_VALID           BIT1
#define EFI_MS_CHECK_UNCORRECTED_VALID               BIT2
#define EFI_MS_CHECK_PRECISE_IP_VALID                BIT3
#define EFI_MS_CHECK_RESTARTABLE_VALID               BIT4
#define EFI_MS_CHECK_OVERFLOW_VALID                  BIT5
///@}

///
/// Error type identifies the operation that caused the error.
///@{
#define EFI_MS_CHECK_ERROR_TYPE_NO                             0
#define EFI_MS_CHECK_ERROR_TYPE_UNCLASSIFIED                   1
#define EFI_MS_CHECK_ERROR_TYPE_MICROCODE_PARITY               2
#define EFI_MS_CHECK_ERROR_TYPE_EXTERNAL                       3
#define EFI_MS_CHECK_ERROR_TYPE_FRC                            4
#define EFI_MS_CHECK_ERROR_TYPE_INTERNAL_UNCLASSIFIED          5
///@}

///
/// IA32/X64 MS Check Field Description
///
typedef struct {
  UINT64             ValidFields:16;
  UINT64             ErrorType:3;
  UINT64             ContextCorrupt:1;
  UINT64             ErrorUncorrected:1;
  UINT64             PreciseIp:1;
  UINT64             RestartableIp:1;
  UINT64             Overflow:1;
  UINT64             Resv1:40;
} EFI_IA32_X64_MS_CHECK_INFO;

///
/// IA32/X64 Check Information Item
///
typedef union {
  EFI_IA32_X64_CACHE_CHECK_INFO  CacheCheck;
  EFI_IA32_X64_TLB_CHECK_INFO    TlbCheck;
  EFI_IA32_X64_BUS_CHECK_INFO    BusCheck;
  EFI_IA32_X64_MS_CHECK_INFO     MsCheck;
  UINT64                         Data64;
} EFI_IA32_X64_CHECK_INFO_ITEM;

///
/// The validation bit mask indicates which fields in the IA32/X64 Processor Error
/// Information Structure are valid.
///@{
#define EFI_IA32_X64_ERROR_PROC_CHECK_INFO_VALID       BIT0
#define EFI_IA32_X64_ERROR_PROC_TARGET_ADDR_VALID      BIT1
#define EFI_IA32_X64_ERROR_PROC_REQUESTER_ID_VALID     BIT2
#define EFI_IA32_X64_ERROR_PROC_RESPONDER_ID_VALID     BIT3
#define EFI_IA32_X64_ERROR_PROC_INST_IP_VALID          BIT4
///@}

///
/// IA32/X64 Processor Error Information Structure
///
typedef struct {
  EFI_GUID                     ErrorType;
  UINT64                       ValidFields;
  EFI_IA32_X64_CHECK_INFO_ITEM CheckInfo;
  UINT64                       TargetId;
  UINT64                       RequestorId;
  UINT64                       ResponderId;
  UINT64                       InstructionIP;
} EFI_IA32_X64_PROCESS_ERROR_INFO;

///
/// IA32/X64 Processor Context Information Structure
///
typedef struct {
  UINT16             RegisterType;
  UINT16             ArraySize;
  UINT32             MsrAddress;
  UINT64             MmRegisterAddress;
  //
  // This field will provide the contents of the actual registers or raw data.
  // The number of Registers or size of the raw data reported is determined
  // by (Array Size / 8) or otherwise specified by the context structure type
  // definition.
  //
} EFI_IA32_X64_PROCESSOR_CONTEXT_INFO;

///
/// Register Context Type
///@{
#define EFI_REG_CONTEXT_TYPE_UNCLASSIFIED            0x0000
#define EFI_REG_CONTEXT_TYPE_MSR                     0x0001
#define EFI_REG_CONTEXT_TYPE_IA32                    0x0002
#define EFI_REG_CONTEXT_TYPE_X64                     0x0003
#define EFI_REG_CONTEXT_TYPE_FXSAVE                  0x0004
#define EFI_REG_CONTEXT_TYPE_DR_IA32                 0x0005
#define EFI_REG_CONTEXT_TYPE_DR_X64                  0x0006
#define EFI_REG_CONTEXT_TYPE_MEM_MAP                 0x0007
///@}

///
/// IA32 Register State
///
typedef struct {
  UINT32             Eax;
  UINT32             Ebx;
  UINT32             Ecx;
  UINT32             Edx;
  UINT32             Esi;
  UINT32             Edi;
  UINT32             Ebp;
  UINT32             Esp;
  UINT16             Cs;
  UINT16             Ds;
  UINT16             Ss;
  UINT16             Es;
  UINT16             Fs;
  UINT16             Gs;
  UINT32             Eflags;
  UINT32             Eip;
  UINT32             Cr0;
  UINT32             Cr1;
  UINT32             Cr2;
  UINT32             Cr3;
  UINT32             Cr4;
  UINT32             Gdtr[2];
  UINT32             Idtr[2];
  UINT16             Ldtr;
  UINT16             Tr;
} EFI_CONTEXT_IA32_REGISTER_STATE;

///
/// X64 Register State
///
typedef struct {
  UINT64             Rax;
  UINT64             Rbx;
  UINT64             Rcx;
  UINT64             Rdx;
  UINT64             Rsi;
  UINT64             Rdi;
  UINT64             Rbp;
  UINT64             Rsp;
  UINT64             R8;
  UINT64             R9;
  UINT64             R10;
  UINT64             R11;
  UINT64             R12;
  UINT64             R13;
  UINT64             R14;
  UINT64             R15;
  UINT16             Cs;
  UINT16             Ds;
  UINT16             Ss;
  UINT16             Es;
  UINT16             Fs;
  UINT16             Gs;
  UINT32             Resv1;
  UINT64             Rflags;
  UINT64             Rip;
  UINT64             Cr0;
  UINT64             Cr1;
  UINT64             Cr2;
  UINT64             Cr3;
  UINT64             Cr4;
  UINT64             Gdtr[2];
  UINT64             Idtr[2];
  UINT16             Ldtr;
  UINT16             Tr;
} EFI_CONTEXT_X64_REGISTER_STATE;

///
/// The validation bit mask indicates each of the following field is in IA32/X64
/// Processor Error Section.
///
typedef struct {
  UINT64             ApicIdValid:1;
  UINT64             CpuIdInforValid:1;
  UINT64             ErrorInfoNum:6;
  UINT64             ContextNum:6;
  UINT64             Resv1:50;
} EFI_IA32_X64_VALID_BITS;

#endif

///
/// Error Status Fields
///
typedef struct {
  UINT64          Resv1:8;
  UINT64          Type:8;
  UINT64          AddressSignal:1;        ///< Error in Address signals or in Address portion of transaction
  UINT64          ControlSignal:1;        ///< Error in Control signals or in Control portion of transaction
  UINT64          DataSignal:1;           ///< Error in Data signals or in Data portion of transaction
  UINT64          DetectedByResponder:1;  ///< Error detected by responder
  UINT64          DetectedByRequester:1;  ///< Error detected by requestor
  UINT64          FirstError:1;           ///< First Error in the sequence - option field
  UINT64          OverflowNotLogged:1;    ///< Additional errors were not logged due to lack of resources
  UINT64          Resv2:41;
} EFI_GENERIC_ERROR_STATUS;

///
/// Error Type
///
typedef enum {
  ///
  /// General Internal errors
  ///
  ErrorInternal       = 1,
  ErrorBus            = 16,
  ///
  /// Component Internal errors
  ///
  ErrorMemStorage     = 4,        // Error in memory device
  ErrorTlbStorage     = 5,        // TLB error in cache
  ErrorCacheStorage   = 6,
  ErrorFunctionalUnit = 7,
  ErrorSelftest       = 8,
  ErrorOverflow       = 9,
  ///
  /// Bus internal errors
  ///
  ErrorVirtualMap     = 17,
  ErrorAccessInvalid  = 18,       // Improper access
  ErrorUnimplAccess   = 19,       // Unimplemented memory access
  ErrorLossOfLockstep = 20,
  ErrorResponseInvalid= 21,       // Response not associated with request
  ErrorParity         = 22,
  ErrorProtocol       = 23,
  ErrorPath           = 24,       // Detected path error
  ErrorTimeout        = 25,       // Bus timeout
  ErrorPoisoned       = 26        // Read data poisoned
} EFI_GENERIC_ERROR_STATUS_ERROR_TYPE;

///
/// Validation bit mask indicates which fields in the memory error record are valid
/// in Memory Error section
///@{
#define EFI_PLATFORM_MEMORY_ERROR_STATUS_VALID                 BIT0
#define EFI_PLATFORM_MEMORY_PHY_ADDRESS_VALID                  BIT1
#define EFI_PLATFORM_MEMORY_PHY_ADDRESS_MASK_VALID             BIT2
#define EFI_PLATFORM_MEMORY_NODE_VALID                         BIT3
#define EFI_PLATFORM_MEMORY_CARD_VALID                         BIT4
#define EFI_PLATFORM_MEMORY_MODULE_VALID                       BIT5
#define EFI_PLATFORM_MEMORY_BANK_VALID                         BIT6
#define EFI_PLATFORM_MEMORY_DEVICE_VALID                       BIT7
#define EFI_PLATFORM_MEMORY_ROW_VALID                          BIT8
#define EFI_PLATFORM_MEMORY_COLUMN_VALID                       BIT9
#define EFI_PLATFORM_MEMORY_BIT_POS_VALID                      BIT10
#define EFI_PLATFORM_MEMORY_REQUESTOR_ID_VALID                 BIT11
#define EFI_PLATFORM_MEMORY_RESPONDER_ID_VALID                 BIT12
#define EFI_PLATFORM_MEMORY_TARGET_ID_VALID                    BIT13
#define EFI_PLATFORM_MEMORY_ERROR_TYPE_VALID                   BIT14
#define EFI_PLATFORM_MEMORY_ERROR_RANK_NUM_VALID               BIT15
#define EFI_PLATFORM_MEMORY_ERROR_CARD_HANDLE_VALID            BIT16
#define EFI_PLATFORM_MEMORY_ERROR_MODULE_HANDLE_VALID          BIT17
#define EFI_PLATFORM_MEMORY_ERROR_EXTENDED_ROW_BIT_16_17_VALID BIT18
#define EFI_PLATFORM_MEMORY_ERROR_BANK_GROUP_VALID             BIT19
#define EFI_PLATFORM_MEMORY_ERROR_BANK_ADDRESS_VALID           BIT20
#define EFI_PLATFORM_MEMORY_ERROR_CHIP_IDENTIFICATION_VALID    BIT21
///@}

///
/// Memory Error Type identifies the type of error that occurred in Memory
/// Error section
///@{
#define EFI_PLATFORM_MEMORY_ERROR_UNKNOWN                      0x00
#define EFI_PLATFORM_MEMORY_ERROR_NONE                         0x01
#define EFI_PLATFORM_MEMORY_ERROR_SINGLEBIT_ECC                0x02
#define EFI_PLATFORM_MEMORY_ERROR_MLTIBIT_ECC                  0x03
#define EFI_PLATFORM_MEMORY_ERROR_SINGLESYMBOLS_CHIPKILL       0x04
#define EFI_PLATFORM_MEMORY_ERROR_MULTISYMBOL_CHIPKILL         0x05
#define EFI_PLATFORM_MEMORY_ERROR_MATER_ABORT                  0x06
#define EFI_PLATFORM_MEMORY_ERROR_TARGET_ABORT                 0x07
#define EFI_PLATFORM_MEMORY_ERROR_PARITY                       0x08
#define EFI_PLATFORM_MEMORY_ERROR_WDT                          0x09
#define EFI_PLATFORM_MEMORY_ERROR_INVALID_ADDRESS              0x0A
#define EFI_PLATFORM_MEMORY_ERROR_MIRROR_FAILED                0x0B
#define EFI_PLATFORM_MEMORY_ERROR_SPARING                      0x0C
#define EFI_PLATFORM_MEMORY_ERROR_SCRUB_CORRECTED              0x0D
#define EFI_PLATFORM_MEMORY_ERROR_SCRUB_UNCORRECTED            0x0E
#define EFI_PLATFORM_MEMORY_ERROR_MEMORY_MAP_EVENT             0x0F
///@}

///
/// Memory Error Section
///
typedef struct {
  UINT64                   ValidFields;
  EFI_GENERIC_ERROR_STATUS ErrorStatus;
  UINT64                   PhysicalAddress;      // Error physical address
  UINT64                   PhysicalAddressMask;  // Grnaularity
  UINT16                   Node;                 // Node #
  UINT16                   Card;
  UINT16                   ModuleRank;           // Module or Rank#
  UINT16                   Bank;
  UINT16                   Device;
  UINT16                   Row;
  UINT16                   Column;
  UINT16                   BitPosition;
  UINT64                   RequestorId;
  UINT64                   ResponderId;
  UINT64                   TargetId;
  UINT8                    ErrorType;
  UINT8                    Extended;
  UINT16                   RankNum;
  UINT16                   CardHandle;
  UINT16                   ModuleHandle;
} EFI_PLATFORM_MEMORY_ERROR_DATA;

///
/// Validation bit mask indicates which fields in the memory error record 2 are valid
/// in Memory Error section 2
///@{
#define EFI_PLATFORM_MEMORY2_ERROR_STATUS_VALID                 BIT0
#define EFI_PLATFORM_MEMORY2_PHY_ADDRESS_VALID                  BIT1
#define EFI_PLATFORM_MEMORY2_PHY_ADDRESS_MASK_VALID             BIT2
#define EFI_PLATFORM_MEMORY2_NODE_VALID                         BIT3
#define EFI_PLATFORM_MEMORY2_CARD_VALID                         BIT4
#define EFI_PLATFORM_MEMORY2_MODULE_VALID                       BIT5
#define EFI_PLATFORM_MEMORY2_BANK_VALID                         BIT6
#define EFI_PLATFORM_MEMORY2_DEVICE_VALID                       BIT7
#define EFI_PLATFORM_MEMORY2_ROW_VALID                          BIT8
#define EFI_PLATFORM_MEMORY2_COLUMN_VALID                       BIT9
#define EFI_PLATFORM_MEMORY2_RANK_VALID                         BIT10
#define EFI_PLATFORM_MEMORY2_BIT_POS_VALID                      BIT11
#define EFI_PLATFORM_MEMORY2_CHIP_ID_VALID                      BIT12
#define EFI_PLATFORM_MEMORY2_MEMORY_ERROR_TYPE_VALID            BIT13
#define EFI_PLATFORM_MEMORY2_STATUS_VALID                       BIT14
#define EFI_PLATFORM_MEMORY2_REQUESTOR_ID_VALID                 BIT15
#define EFI_PLATFORM_MEMORY2_RESPONDER_ID_VALID                 BIT16
#define EFI_PLATFORM_MEMORY2_TARGET_ID_VALID                    BIT17
#define EFI_PLATFORM_MEMORY2_CARD_HANDLE_VALID                  BIT18
#define EFI_PLATFORM_MEMORY2_MODULE_HANDLE_VALID                BIT19
#define EFI_PLATFORM_MEMORY2_BANK_GROUP_VALID                   BIT20
#define EFI_PLATFORM_MEMORY2_BANK_ADDRESS_VALID                 BIT21
///@}

///
/// Memory Error Type identifies the type of error that occurred in Memory
/// Error section 2
///@{
#define EFI_PLATFORM_MEMORY2_ERROR_UNKNOWN                      0x00
#define EFI_PLATFORM_MEMORY2_ERROR_NONE                         0x01
#define EFI_PLATFORM_MEMORY2_ERROR_SINGLEBIT_ECC                0x02
#define EFI_PLATFORM_MEMORY2_ERROR_MLTIBIT_ECC                  0x03
#define EFI_PLATFORM_MEMORY2_ERROR_SINGLESYMBOL_CHIPKILL        0x04
#define EFI_PLATFORM_MEMORY2_ERROR_MULTISYMBOL_CHIPKILL         0x05
#define EFI_PLATFORM_MEMORY2_ERROR_MASTER_ABORT                 0x06
#define EFI_PLATFORM_MEMORY2_ERROR_TARGET_ABORT                 0x07
#define EFI_PLATFORM_MEMORY2_ERROR_PARITY                       0x08
#define EFI_PLATFORM_MEMORY2_ERROR_WDT                          0x09
#define EFI_PLATFORM_MEMORY2_ERROR_INVALID_ADDRESS              0x0A
#define EFI_PLATFORM_MEMORY2_ERROR_MIRROR_BROKEN                0x0B
#define EFI_PLATFORM_MEMORY2_ERROR_MEMORY_SPARING               0x0C
#define EFI_PLATFORM_MEMORY2_ERROR_SCRUB_CORRECTED              0x0D
#define EFI_PLATFORM_MEMORY2_ERROR_SCRUB_UNCORRECTED            0x0E
#define EFI_PLATFORM_MEMORY2_ERROR_MEMORY_MAP_EVENT             0x0F
///@}

///
/// Memory Error Section 2
///
typedef struct {
  UINT64                    ValidFields;
  EFI_GENERIC_ERROR_STATUS  ErrorStatus;
  UINT64                    PhysicalAddress;      // Error physical address
  UINT64                    PhysicalAddressMask;  // Grnaularity
  UINT16                    Node;                 // Node #
  UINT16                    Card;
  UINT16                    Module;               // Module or Rank#
  UINT16                    Bank;
  UINT32                    Device;
  UINT32                    Row;
  UINT32                    Column;
  UINT32                    Rank;
  UINT32                    BitPosition;
  UINT8                     ChipId;
  UINT8                     MemErrorType;
  UINT8                     Status;
  UINT8                     Reserved;
  UINT64                    RequestorId;
  UINT64                    ResponderId;
  UINT64                    TargetId;
  UINT32                    CardHandle;
  UINT32                    ModuleHandle;
} EFI_PLATFORM_MEMORY2_ERROR_DATA;

///
/// Validation bits mask indicates which of the following fields is valid
/// in PCI Express Error Record.
///@{
#define EFI_PCIE_ERROR_PORT_TYPE_VALID               BIT0
#define EFI_PCIE_ERROR_VERSION_VALID                 BIT1
#define EFI_PCIE_ERROR_COMMAND_STATUS_VALID          BIT2
#define EFI_PCIE_ERROR_DEVICE_ID_VALID               BIT3
#define EFI_PCIE_ERROR_SERIAL_NO_VALID               BIT4
#define EFI_PCIE_ERROR_BRIDGE_CRL_STS_VALID          BIT5
#define EFI_PCIE_ERROR_CAPABILITY_INFO_VALID         BIT6
#define EFI_PCIE_ERROR_AER_INFO_VALID                BIT7
///@}

///
/// PCIe Device/Port Type as defined in the PCI Express capabilities register
///@{
#define EFI_PCIE_ERROR_PORT_PCIE_ENDPOINT            0x00000000
#define EFI_PCIE_ERROR_PORT_PCI_ENDPOINT             0x00000001
#define EFI_PCIE_ERROR_PORT_ROOT_PORT                0x00000004
#define EFI_PCIE_ERROR_PORT_UPSWITCH_PORT            0x00000005
#define EFI_PCIE_ERROR_PORT_DOWNSWITCH_PORT          0x00000006
#define EFI_PCIE_ERROR_PORT_PCIE_TO_PCI_BRIDGE       0x00000007
#define EFI_PCIE_ERROR_PORT_PCI_TO_PCIE_BRIDGE       0x00000008
#define EFI_PCIE_ERROR_PORT_ROOT_INT_ENDPOINT        0x00000009
#define EFI_PCIE_ERROR_PORT_ROOT_EVENT_COLLECTOR     0x0000000A
///@}

///
/// PCI Slot number
///
typedef struct {
  UINT16          Resv1:3;
  UINT16          Number:13;
} EFI_GENERIC_ERROR_PCI_SLOT;

///
/// PCIe Root Port PCI/bridge PCI compatible device number and
/// bus number information to uniquely identify the root port or
/// bridge. Default values for both the bus numbers is zero.
///
typedef struct {
  UINT16                     VendorId;
  UINT16                     DeviceId;
  UINT8                      ClassCode[3];
  UINT8                      Function;
  UINT8                      Device;
  UINT16                     Segment;
  UINT8                      PrimaryOrDeviceBus;
  UINT8                      SecondaryBus;
  EFI_GENERIC_ERROR_PCI_SLOT Slot;
  UINT8                      Resv1;
} EFI_GENERIC_ERROR_PCIE_DEV_BRIDGE_ID;

///
/// PCIe Capability Structure
///
typedef struct {
  UINT8           PcieCap[60];
} EFI_PCIE_ERROR_DATA_CAPABILITY;

///
/// PCIe Advanced Error Reporting Extended Capability Structure.
///
typedef struct {
  UINT8           PcieAer[96];
} EFI_PCIE_ERROR_DATA_AER;

///
/// PCI Express Error Record
///
typedef struct {
  UINT64                               ValidFields;
  UINT32                               PortType;
  UINT32                               Version;
  UINT32                               CommandStatus;
  UINT32                               Resv2;
  EFI_GENERIC_ERROR_PCIE_DEV_BRIDGE_ID DevBridge;
  UINT64                               SerialNo;
  UINT32                               BridgeControlStatus;
  EFI_PCIE_ERROR_DATA_CAPABILITY       Capability;
  EFI_PCIE_ERROR_DATA_AER              AerInfo;
} EFI_PCIE_ERROR_DATA;

///
/// Validation bits Indicates which of the following fields is valid
/// in PCI/PCI-X Bus Error Section.
///@{
#define EFI_PCI_PCIX_BUS_ERROR_STATUS_VALID          BIT0
#define EFI_PCI_PCIX_BUS_ERROR_TYPE_VALID            BIT1
#define EFI_PCI_PCIX_BUS_ERROR_BUS_ID_VALID          BIT2
#define EFI_PCI_PCIX_BUS_ERROR_BUS_ADDRESS_VALID     BIT3
#define EFI_PCI_PCIX_BUS_ERROR_BUS_DATA_VALID        BIT4
#define EFI_PCI_PCIX_BUS_ERROR_COMMAND_VALID         BIT5
#define EFI_PCI_PCIX_BUS_ERROR_REQUESTOR_ID_VALID    BIT6
#define EFI_PCI_PCIX_BUS_ERROR_COMPLETER_ID_VALID    BIT7
#define EFI_PCI_PCIX_BUS_ERROR_TARGET_ID_VALID       BIT8
///@}

///
/// PCI Bus Error Type in PCI/PCI-X Bus Error Section
///@{
#define EFI_PCI_PCIX_BUS_ERROR_UNKNOWN               0x0000
#define EFI_PCI_PCIX_BUS_ERROR_DATA_PARITY           0x0001
#define EFI_PCI_PCIX_BUS_ERROR_SYSTEM                0x0002
#define EFI_PCI_PCIX_BUS_ERROR_MASTER_ABORT          0x0003
#define EFI_PCI_PCIX_BUS_ERROR_BUS_TIMEOUT           0x0004
#define EFI_PCI_PCIX_BUS_ERROR_MASTER_DATA_PARITY    0x0005
#define EFI_PCI_PCIX_BUS_ERROR_ADDRESS_PARITY        0x0006
#define EFI_PCI_PCIX_BUS_ERROR_COMMAND_PARITY        0x0007
///@}

///
/// PCI/PCI-X Bus Error Section
///
typedef struct {
  UINT64                   ValidFields;
  EFI_GENERIC_ERROR_STATUS ErrorStatus;
  UINT16                   Type;
  UINT16                   BusId;
  UINT32                   Resv2;
  UINT64                   BusAddress;
  UINT64                   BusData;
  UINT64                   BusCommand;
  UINT64                   RequestorId;
  UINT64                   ResponderId;
  UINT64                   TargetId;
} EFI_PCI_PCIX_BUS_ERROR_DATA;

///
/// Validation bits Indicates which of the following fields is valid
/// in PCI/PCI-X Component Error Section.
///@{
#define EFI_PCI_PCIX_DEVICE_ERROR_STATUS_VALID                 BIT0
#define EFI_PCI_PCIX_DEVICE_ERROR_ID_INFO_VALID                BIT1
#define EFI_PCI_PCIX_DEVICE_ERROR_MEM_NUM_VALID                BIT2
#define EFI_PCI_PCIX_DEVICE_ERROR_IO_NUM_VALID                 BIT3
#define EFI_PCI_PCIX_DEVICE_ERROR_REG_DATA_PAIR_VALID          BIT4
///@}

///
/// PCI/PCI-X Device Identification Information
///
typedef struct {
  UINT16          VendorId;
  UINT16          DeviceId;
  UINT8           ClassCode[3];
  UINT8           Function;
  UINT8           Device;
  UINT8           Bus;
  UINT8           Segment;
  UINT8           Resv1;
  UINT32          Resv2;
} EFI_GENERIC_ERROR_PCI_DEVICE_ID;

///
/// Identifies the type of firmware error record
///@{
#define EFI_FIRMWARE_ERROR_TYPE_IPF_SAL              0x00
#define EFI_FIRMWARE_ERROR_TYPE_SOC_TYPE1            0x01
#define EFI_FIRMWARE_ERROR_TYPE_SOC_TYPE2            0x02
///@}

///
/// Firmware Error Record Section
///
typedef struct {
  UINT8       ErrorType;
  UINT8       Revision;
  UINT8       Resv1[6];
  UINT64      RecordId;
  EFI_GUID    RecordIdGuid;
} EFI_FIRMWARE_ERROR_DATA;

///
/// Fault Reason in DMAr Generic Error Section
///@{
#define EFI_DMA_FAULT_REASON_TABLE_ENTRY_NOT_PRESENT           0x01
#define EFI_DMA_FAULT_REASON_TABLE_ENTRY_INVALID               0x02
#define EFI_DMA_FAULT_REASON_ACCESS_MAPPING_TABLE_ERROR        0x03
#define EFI_DMA_FAULT_REASON_RESV_BIT_ERROR_IN_MAPPING_TABLE   0x04
#define EFI_DMA_FAULT_REASON_ACCESS_ADDR_OUT_OF_SPACE          0x05
#define EFI_DMA_FAULT_REASON_INVALID_ACCESS                    0x06
#define EFI_DMA_FAULT_REASON_INVALID_REQUEST                   0x07
#define EFI_DMA_FAULT_REASON_ACCESS_TRANSLATE_TABLE_ERROR      0x08
#define EFI_DMA_FAULT_REASON_RESV_BIT_ERROR_IN_TRANSLATE_TABLE 0x09
#define EFI_DMA_FAULT_REASON_INVALID_COMMAOND                  0x0A
#define EFI_DMA_FAULT_REASON_ACCESS_COMMAND_BUFFER_ERROR       0x0B
///@}

///
/// DMA access type in DMAr Generic Error Section
///@{
#define EFI_DMA_ACCESS_TYPE_READ                     0x00
#define EFI_DMA_ACCESS_TYPE_WRITE                    0x01
///@}

///
/// DMA address type in DMAr Generic Error Section
///@{
#define EFI_DMA_ADDRESS_UNTRANSLATED                 0x00
#define EFI_DMA_ADDRESS_TRANSLATION                  0x01
///@}

///
/// Architecture type in DMAr Generic Error Section
///@{
#define EFI_DMA_ARCH_TYPE_VT                         0x01
#define EFI_DMA_ARCH_TYPE_IOMMU                      0x02
///@}

///
/// DMAr Generic Error Section
///
typedef struct {
  UINT16      RequesterId;
  UINT16      SegmentNumber;
  UINT8       FaultReason;
  UINT8       AccessType;
  UINT8       AddressType;
  UINT8       ArchType;
  UINT64      DeviceAddr;
  UINT8       Resv1[16];
} EFI_DMAR_GENERIC_ERROR_DATA;

///
/// Intel VT for Directed I/O specific DMAr Errors
///
typedef struct {
  UINT8           Version;
  UINT8           Revision;
  UINT8           OemId[6];
  UINT64          Capability;
  UINT64          CapabilityEx;
  UINT32          GlobalCommand;
  UINT32          GlobalStatus;
  UINT32          FaultStatus;
  UINT8           Resv1[12];
  UINT64          FaultRecord[2];
  UINT64          RootEntry[2];
  UINT64          ContextEntry[2];
  UINT64          PteL6;
  UINT64          PteL5;
  UINT64          PteL4;
  UINT64          PteL3;
  UINT64          PteL2;
  UINT64          PteL1;
} EFI_DIRECTED_IO_DMAR_ERROR_DATA;

///
/// IOMMU specific DMAr Errors
///
typedef struct {
  UINT8           Revision;
  UINT8           Resv1[7];
  UINT64          Control;
  UINT64          Status;
  UINT8           Resv2[8];
  UINT64          EventLogEntry[2];
  UINT8           Resv3[16];
  UINT64          DeviceTableEntry[4];
  UINT64          PteL6;
  UINT64          PteL5;
  UINT64          PteL4;
  UINT64          PteL3;
  UINT64          PteL2;
  UINT64          PteL1;
} EFI_IOMMU_DMAR_ERROR_DATA;

#pragma pack()

extern EFI_GUID gEfiEventNotificationTypeCmcGuid;
extern EFI_GUID gEfiEventNotificationTypeCpeGuid;
extern EFI_GUID gEfiEventNotificationTypeMceGuid;
extern EFI_GUID gEfiEventNotificationTypePcieGuid;
extern EFI_GUID gEfiEventNotificationTypeInitGuid;
extern EFI_GUID gEfiEventNotificationTypeNmiGuid;
extern EFI_GUID gEfiEventNotificationTypeBootGuid;
extern EFI_GUID gEfiEventNotificationTypeDmarGuid;
extern EFI_GUID gEfiEventNotificationTypeSeaGuid;
extern EFI_GUID gEfiEventNotificationTypeSeiGuid;
extern EFI_GUID gEfiEventNotificationTypePeiGuid;

extern EFI_GUID gEfiProcessorGenericErrorSectionGuid;
extern EFI_GUID gEfiProcessorSpecificErrorSectionGuid;
extern EFI_GUID gEfiIa32X64ProcessorErrorSectionGuid;
extern EFI_GUID gEfiArmProcessorErrorSectionGuid ;
extern EFI_GUID gEfiPlatformMemoryErrorSectionGuid;
extern EFI_GUID gEfiPlatformMemory2ErrorSectionGuid;
extern EFI_GUID gEfiPcieErrorSectionGuid;
extern EFI_GUID gEfiFirmwareErrorSectionGuid;
extern EFI_GUID gEfiPciBusErrorSectionGuid;
extern EFI_GUID gEfiPciDevErrorSectionGuid;
extern EFI_GUID gEfiDMArGenericErrorSectionGuid;
extern EFI_GUID gEfiDirectedIoDMArErrorSectionGuid;
extern EFI_GUID gEfiIommuDMArErrorSectionGuid;

#if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64)
///
/// IA32 and x64 Specific definitions.
///

extern EFI_GUID gEfiIa32X64ErrorTypeCacheCheckGuid;
extern EFI_GUID gEfiIa32X64ErrorTypeTlbCheckGuid;
extern EFI_GUID gEfiIa32X64ErrorTypeBusCheckGuid;
extern EFI_GUID gEfiIa32X64ErrorTypeMsCheckGuid;

#endif

#endif
