/** @file
  ACPI IO Remapping Table (IORT) as specified in ARM spec DEN0049D

  http://infocenter.arm.com/help/topic/com.arm.doc.den0049d/DEN0049D_IO_Remapping_Table.pdf

  Copyright (c) 2017, Linaro Limited. All rights reserved.<BR>
  Copyright (c) 2018, ARM Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __IO_REMAPPING_TABLE_H__
#define __IO_REMAPPING_TABLE_H__

#include <IndustryStandard/Acpi.h>

#define EFI_ACPI_IO_REMAPPING_TABLE_REVISION        0x0

#define EFI_ACPI_IORT_TYPE_ITS_GROUP                0x0
#define EFI_ACPI_IORT_TYPE_NAMED_COMP               0x1
#define EFI_ACPI_IORT_TYPE_ROOT_COMPLEX             0x2
#define EFI_ACPI_IORT_TYPE_SMMUv1v2                 0x3
#define EFI_ACPI_IORT_TYPE_SMMUv3                   0x4
#define EFI_ACPI_IORT_TYPE_PMCG                     0x5

#define EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA           BIT0

#define EFI_ACPI_IORT_MEM_ACCESS_PROP_AH_TR         BIT0
#define EFI_ACPI_IORT_MEM_ACCESS_PROP_AH_WA         BIT1
#define EFI_ACPI_IORT_MEM_ACCESS_PROP_AH_RA         BIT2
#define EFI_ACPI_IORT_MEM_ACCESS_PROP_AH_AHO        BIT3

#define EFI_ACPI_IORT_MEM_ACCESS_FLAGS_CPM          BIT0
#define EFI_ACPI_IORT_MEM_ACCESS_FLAGS_DACS         BIT1

#define EFI_ACPI_IORT_SMMUv1v2_MODEL_v1             0x0
#define EFI_ACPI_IORT_SMMUv1v2_MODEL_v2             0x1
#define EFI_ACPI_IORT_SMMUv1v2_MODEL_MMU400         0x2
#define EFI_ACPI_IORT_SMMUv1v2_MODEL_MMU500         0x3
#define EFI_ACPI_IORT_SMMUv1v2_MODEL_MMU401         0x4
#define EFI_ACPI_IORT_SMMUv1v2_MODEL_CAVIUM_THX_v2  0x5

#define EFI_ACPI_IORT_SMMUv1v2_FLAG_DVM             BIT0
#define EFI_ACPI_IORT_SMMUv1v2_FLAG_COH_WALK        BIT1

#define EFI_ACPI_IORT_SMMUv1v2_INT_FLAG_LEVEL       0x0
#define EFI_ACPI_IORT_SMMUv1v2_INT_FLAG_EDGE        0x1

#define EFI_ACPI_IORT_SMMUv3_FLAG_COHAC_OVERRIDE    BIT0
#define EFI_ACPI_IORT_SMMUv3_FLAG_HTTU_OVERRIDE     BIT1
#define EFI_ACPI_IORT_SMMUv3_FLAG_PROXIMITY_DOMAIN  BIT3

#define EFI_ACPI_IORT_SMMUv3_MODEL_GENERIC          0x0
#define EFI_ACPI_IORT_SMMUv3_MODEL_HISILICON_HI161X 0x1
#define EFI_ACPI_IORT_SMMUv3_MODEL_CAVIUM_CN99XX    0x2

#define EFI_ACPI_IORT_ROOT_COMPLEX_ATS_UNSUPPORTED  0x0
#define EFI_ACPI_IORT_ROOT_COMPLEX_ATS_SUPPORTED    0x1

#define EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE       BIT0

#pragma pack(1)

///
/// Table header
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER             Header;
  UINT32                                  NumNodes;
  UINT32                                  NodeOffset;
  UINT32                                  Reserved;
} EFI_ACPI_6_0_IO_REMAPPING_TABLE;

///
/// Definition for ID mapping table shared by all node types
///
typedef struct {
  UINT32                                  InputBase;
  UINT32                                  NumIds;
  UINT32                                  OutputBase;
  UINT32                                  OutputReference;
  UINT32                                  Flags;
} EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE;

///
/// Node header definition shared by all node types
///
typedef struct {
  UINT8                                   Type;
  UINT16                                  Length;
  UINT8                                   Revision;
  UINT32                                  Reserved;
  UINT32                                  NumIdMappings;
  UINT32                                  IdReference;
} EFI_ACPI_6_0_IO_REMAPPING_NODE;

///
/// Node type 0: ITS node
///
typedef struct {
  EFI_ACPI_6_0_IO_REMAPPING_NODE          Node;

  UINT32                                  NumItsIdentifiers;
//UINT32                                  ItsIdentifiers[NumItsIdentifiers];
} EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE;

///
/// Node type 1: root complex node
///
typedef struct {
  EFI_ACPI_6_0_IO_REMAPPING_NODE          Node;

  UINT32                                  CacheCoherent;
  UINT8                                   AllocationHints;
  UINT16                                  Reserved;
  UINT8                                   MemoryAccessFlags;

  UINT32                                  AtsAttribute;
  UINT32                                  PciSegmentNumber;
  UINT8                                   MemoryAddressSize;
  UINT8                                   Reserved1[3];
} EFI_ACPI_6_0_IO_REMAPPING_RC_NODE;

///
/// Node type 2: named component node
///
typedef struct {
  EFI_ACPI_6_0_IO_REMAPPING_NODE          Node;

  UINT32                                  Flags;
  UINT32                                  CacheCoherent;
  UINT8                                   AllocationHints;
  UINT16                                  Reserved;
  UINT8                                   MemoryAccessFlags;
  UINT8                                   AddressSizeLimit;
//UINT8                                   ObjectName[];
} EFI_ACPI_6_0_IO_REMAPPING_NAMED_COMP_NODE;

///
/// Node type 3: SMMUv1 or SMMUv2 node
///
typedef struct {
  UINT32                                  Interrupt;
  UINT32                                  InterruptFlags;
} EFI_ACPI_6_0_IO_REMAPPING_SMMU_INT;

typedef struct {
  EFI_ACPI_6_0_IO_REMAPPING_NODE          Node;

  UINT64                                  Base;
  UINT64                                  Span;
  UINT32                                  Model;
  UINT32                                  Flags;
  UINT32                                  GlobalInterruptArrayRef;
  UINT32                                  NumContextInterrupts;
  UINT32                                  ContextInterruptArrayRef;
  UINT32                                  NumPmuInterrupts;
  UINT32                                  PmuInterruptArrayRef;

  UINT32                                  SMMU_NSgIrpt;
  UINT32                                  SMMU_NSgIrptFlags;
  UINT32                                  SMMU_NSgCfgIrpt;
  UINT32                                  SMMU_NSgCfgIrptFlags;

//EFI_ACPI_6_0_IO_REMAPPING_SMMU_CTX_INT  ContextInterrupt[NumContextInterrupts];
//EFI_ACPI_6_0_IO_REMAPPING_SMMU_CTX_INT  PmuInterrupt[NumPmuInterrupts];
} EFI_ACPI_6_0_IO_REMAPPING_SMMU_NODE;

///
/// Node type 4: SMMUv3 node
///
typedef struct {
  EFI_ACPI_6_0_IO_REMAPPING_NODE          Node;

  UINT64                                  Base;
  UINT32                                  Flags;
  UINT32                                  Reserved;
  UINT64                                  VatosAddress;
  UINT32                                  Model;
  UINT32                                  Event;
  UINT32                                  Pri;
  UINT32                                  Gerr;
  UINT32                                  Sync;
  UINT32                                  ProximityDomain;
  UINT32                                  DeviceIdMappingIndex;
} EFI_ACPI_6_0_IO_REMAPPING_SMMU3_NODE;

///
/// Node type 5: PMCG node
///
typedef struct {
  EFI_ACPI_6_0_IO_REMAPPING_NODE          Node;

  UINT64                                  Base;
  UINT32                                  OverflowInterruptGsiv;
  UINT32                                  NodeReference;
  UINT64                                  Page1Base;
//EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE      OverflowInterruptMsiMapping[1];
} EFI_ACPI_6_0_IO_REMAPPING_PMCG_NODE;

#pragma pack()

#endif
